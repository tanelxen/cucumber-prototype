//
//  TrenchBroomMapComponent.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 21.10.26.
//

#include "MapComponent.h"
#include "Entity.h"
#include "LightComponent.h"
#include "ProjectManager.h"
#include "PhysicsSystem.h"
#include "World.h"

#include "TextureLibrary.h"

#include "trenchbroom/MapAsset.h"
#include "trenchbroom/LightmapBaker.h"
#include "trenchbroom/MapMesh.h"
#include "trenchbroom/MapImporter.h"
#include "trenchbroom/LevelCooker.h"
#include "level/LevelDocumentIO.h"

#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include <iostream>
#include <cmath>

namespace
{
    bool IsLevelFile(const std::string& p)
    {
        constexpr std::string_view ext = ".cmap.json";
        return p.size() >= ext.size() && p.compare(p.size() - ext.size(), ext.size(), ext) == 0;
    }
    
    std::filesystem::path LevelPathFor(std::filesystem::path mapFile)
    {
        return mapFile.replace_extension(".cmap.json");   // maps/e1m1.map -> maps/e1m1.cmap.json
    }
}

bool MapComponent::createEmptyLevel(const std::filesystem::path& relativePath)
{
    if (m_edit) { bakeStatus = "Exit edit mode first"; return false; }
    
    const auto absolute = ProjectManager::Get().assetsPath() / relativePath;
    if (std::filesystem::exists(absolute)) { bakeStatus = "File already exists: " + relativePath.string(); return false; }
    
    std::error_code ec;
    std::filesystem::create_directories(absolute.parent_path(), ec);
    
    std::string error;
    if (!LevelDocumentIO::Save(absolute, LevelDocument{}, error)) { bakeStatus = "Cannot create level: " + error; return false; }
    
    return loadLevel(relativePath);   // выставит path, basePath и пустой меш
}

bool MapComponent::loadTrenchBroomMap(const std::filesystem::path& filepath)
{
    if (m_edit) { bakeStatus = "Exit edit mode first"; return false; }
    return IsLevelFile(filepath.string()) ? loadLevel(filepath) : importTrenchBroomMap(filepath, false);
}

bool MapComponent::rebuildLightmapUVs()
{
    if (level.solids.empty()) { bakeStatus = "Cannot rebuild lightmap UVs: no level is loaded"; return false; }
    if (!applyLevel()) return false;
    bakeStatus = "Lightmap UVs rebuilt; bake lighting again";
    return true;
}

bool MapComponent::bakeLighting()
{
    if (!mesh || !owner || !owner->scene)
    {
        bakeStatus = "Cannot bake lighting: map is not loaded into a scene";
        return false;
    }
    
    return lightmap.startBake(collectBakeLights());
}

std::vector<LightmapBakeLight> MapComponent::collectBakeLights()
{
    std::vector<LightmapBakeLight> bakeLights;
    
    for (const auto& entity : owner->scene->m_entities)
    {
        if (!entity || !entity->m_isActive) continue;
        const auto* component = entity->getComponent<LightComponent>();
        if (!component || !component->enabled
            || (component->mode != LightMode::Baked
                && component->mode != LightMode::BakedAndRealtime))
        {
            continue;
        }
        
        LightmapBakeLight light;
        light.type = static_cast<LightmapBakeLightType>(component->type);
        light.position = entity->getTransform().getWorldPosition();
        light.direction = entity->getTransform().getWorldRotation() * glm::vec3(0.0f, 0.0f, -1.0f);
        light.color = component->color;
        light.intensity = component->intensity;
        light.range = component->range;
        light.spotAngle = component->spotAngle;
        bakeLights.push_back(light);
    }
    
    return bakeLights;
}

bool MapComponent::isLightmapOutdated()
{
    if (!mesh || !owner || !owner->scene) return false;
    return lightmap.isOutdated(collectBakeLights());
}

void MapComponent::destroyPhysicsBody()
{
    if (!m_bodyID.IsInvalid())
    {
        auto& bodyInterface = PhysicsSystem::Get().GetBodyInterface();
        bodyInterface.RemoveBody(m_bodyID);
        bodyInterface.DestroyBody(m_bodyID);
        m_bodyID = JPH::BodyID();
    }
}

MapComponent::~MapComponent()
{
    destroyPhysicsBody();
}

void MapComponent::save(nlohmann::json& j) const
{
    j["source_map"] = path;
    j["lightmap_texels_per_unit"] = lightmapTexelsPerUnit;
}

void MapComponent::load(const nlohmann::json& j)
{
    if (j.contains("source_map"))
    {
        path = j.at("source_map");
    }
    if (j.contains("lightmap_texels_per_unit"))
    {
        lightmapTexelsPerUnit = j.at("lightmap_texels_per_unit");
    }
}

std::filesystem::path MapComponent::lightmapCachePath() const
{
    return ProjectManager::Get().assetsPath() / (path + ".lightmap");
}

bool MapComponent::importTrenchBroomMap(const std::filesystem::path& mapFile, bool overwrite)
{
    if (m_edit) { bakeStatus = "Exit edit mode first"; return false; }
    
    const auto assets = ProjectManager::Get().assetsPath();
    const auto levelRel = LevelPathFor(mapFile);
    
    // Уже есть документ: не затираем правки редактора
    if (!overwrite && std::filesystem::exists(assets / levelRel)) return loadLevel(levelRel);
    
    MapAsset mapData;
    if (!mapData.loadFromFile((assets / mapFile).string()))
    {
        bakeStatus = "Cannot open or parse .map file";
        return false;
    }
    
    LevelDocument imported;
    MapImportReport report;
    std::string error;
    if (!ImportMapAsset(mapData, imported, report, error))
    {
        bakeStatus = "Map import failed: " + error;
        return false;
    }
    
    std::cout << "[Import] brushes " << report.brushesImported << "/" << report.brushesRead
    << ", skipped " << report.brushesSkipped << ", faces dropped " << report.facesDropped
    << ", entities ignored " << report.entitiesIgnored << std::endl;
    for (const auto& warning : report.warnings) std::cout << "[Import] " << warning << std::endl;
    
    // generic_string: слеши одинаковые на всех платформах
    imported.sourceMap = mapFile.generic_string();
    
    if (!LevelDocumentIO::Save(assets / levelRel, imported, error))
    {
        bakeStatus = "Cannot save level: " + error;
        return false;
    }
    return loadLevel(levelRel);
}

bool MapComponent::reimportSource()
{
    if (level.sourceMap.empty())
    {
        bakeStatus = "This level was not imported from a .map";
        return false;
    }
    return importTrenchBroomMap(level.sourceMap, /*overwrite*/ true);
}

bool MapComponent::loadLevel(const std::filesystem::path& filepath)
{
    if (m_edit) { bakeStatus = "Exit edit mode first"; return false; }
    
    const auto absolute = ProjectManager::Get().assetsPath() / filepath;
    
    LevelDocument loaded;
    std::string error;
    if (!LevelDocumentIO::Load(absolute, loaded, error))
    {
        bakeStatus = "Level load failed: " + error;
        return false;
    }

    loaded.basePath = absolute.parent_path().parent_path();   // теперь нужен только для миграции
    
    bool migrated = false;
    if (loaded.version < 2)
    {
        const auto texturesRoot = loaded.basePath / "textures";
        
        for (std::string& material : loaded.materials)
        {
            material = TextureLibrary::CanonicalName(material, texturesRoot);
        }
        
        loaded.version = LevelDocument::kCurrentVersion;
        migrated = true;
    }
    
    path = filepath.string();
    level = std::move(loaded);
    
    if (!applyLevel()) return false;
    if (migrated) m_levelDirty = true;   // уровень сохранится уже в новом формате
    
    return true;
}

bool MapComponent::saveLevel()
{
    if (!IsLevelFile(path))
    {
        bakeStatus = "Nothing to save: no .cmap.json is loaded";
        return false;
    }
    
    std::string error;
    if (!LevelDocumentIO::Save(ProjectManager::Get().assetsPath() / path, level, error))
    {
        bakeStatus = "Save failed: " + error;
        return false;
    }
    
    m_levelDirty = false;
    if (m_edit) m_edit->history().markSaved();
    
    return true;
}

bool MapComponent::applyLevel() { return rebuildMesh(true) && rebuildPhysicsBody(); }

bool MapComponent::rebuildFromDocument() { return applyLevel(); }

bool MapComponent::rebuildPhysicsBody()
{
    destroyPhysicsBody();
    
    std::vector<std::vector<glm::vec3>> hulls;
    LevelCooker::collisionHulls(level, hulls);
    
    if (hulls.empty()) return true;
    
    JPH::StaticCompoundShapeSettings compoundSettings;
    JPH::Array<JPH::Vec3> points;
    int rejected = 0;
    
    for (const auto& hull : hulls)
    {
        points.clear();
        for (const glm::vec3& v : hull) points.push_back(QuakeToJolt(v));
        
        JPH::ConvexHullShapeSettings convexSettings(points);
        convexSettings.mMaxConvexRadius = 0.01f;
        
        JPH::Shape::ShapeResult shapeResult = convexSettings.Create();
        if (shapeResult.IsValid())
            compoundSettings.AddShape(JPH::Vec3::sZero(), JPH::Quat::sIdentity(), shapeResult.Get());
        else
            ++rejected;
    }
    if (rejected > 0) std::cout << "[Physics] Jolt rejected " << rejected << " hulls" << std::endl;
    
    // Финализация физического тела в Jolt
    JPH::Shape::ShapeResult finalShapeResult = compoundSettings.Create();
    if (finalShapeResult.HasError()) {
        std::cout << "[Physics Error] Map compound shape error: " << finalShapeResult.GetError() << std::endl;
        return false;
    }
    
    auto& bodyInterface = PhysicsSystem::Get().GetBodyInterface();
    JPH::BodyCreationSettings settings;
    settings.SetShape(finalShapeResult.Get());
    settings.mMotionType = JPH::EMotionType::Static;
    settings.mObjectLayer = Layers::STATIC;
    settings.mFriction = 0.3f;
    
    JPH::Body* body = bodyInterface.CreateBody(settings);
    m_bodyID = body->GetID();
    
    m_userData.entity = owner;
    m_userData.component = this;
    bodyInterface.SetUserData(m_bodyID, reinterpret_cast<JPH::uint64>(&m_userData));
    bodyInterface.AddBody(m_bodyID, JPH::EActivation::DontActivate);
    
    std::cout << "[Physics] Successfully loaded TrenchBroom .map scene with " << compoundSettings.mSubShapes.size() << " static brushes." << std::endl;
    return true;
}

bool MapComponent::beginEdit()
{
    if (m_edit) return true;
    if (isBaking()) { bakeStatus = "Cancel baking first"; return false; }
    if (owner && owner->scene && owner->scene->isSimulationMode()) return false;
    
    if (path == "<no map>")   // пустой компонент: заводим новый уровень рядом с остальными
    {
        const auto assets = ProjectManager::Get().assetsPath();
        std::filesystem::path candidate;
        for (int i = 0;; ++i)
        {
            candidate = std::string("maps/untitled") + (i ? "_" + std::to_string(i) : "") + ".cmap.json";
            if (!std::filesystem::exists(assets / candidate)) break;
        }
        if (!createEmptyLevel(candidate)) return false;
    }
    
    m_editChanged = false;
    m_editMesh = std::make_unique<LevelEditMesh>();
    m_editMesh->build(level);
    m_edit = std::make_unique<LevelEditSession>(level, [this](const ChangeSet& changes)
                                                {
        m_editChanged = true;
        if (m_editMesh) m_editMesh->apply(level, changes);
    });
    
    bakeStatus = "Editing geometry";
    return true;
}

void MapComponent::endEdit()
{
    if (!m_edit) return;
    if (m_edit->history().isDirty()) m_levelDirty = true;
    m_edit.reset();
    m_editMesh.reset();                // до applyLevel: статика вернётся к запечённому MapMesh
    bakeStatus.clear();
    if (m_editChanged) { m_editChanged = false; applyLevel(); }
}

bool MapComponent::rebuildMesh(bool attachLightmap)
{
    if (level.solids.empty())
    {
        mesh.reset();
        if (attachLightmap) lightmap.attach(nullptr, lightmapCachePath(), 1);
        return true;
    }
    auto newMesh = std::make_shared<MapMesh>();
    LightmapUnwrapSettings unwrap;
    unwrap.texelsPerUnit = lightmapTexelsPerUnit;
    if (!newMesh->buildFromLevel(level, unwrap)) { bakeStatus = "Map mesh build failed"; return false; }
    mesh = std::move(newMesh);
    if (attachLightmap) { bakeStatus.clear(); lightmap.attach(mesh, lightmapCachePath(), unwrap.padding); }
    return true;
}
