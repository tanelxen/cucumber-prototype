//
//  TrenchBroomMapComponent.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 21.10.26.
//

#include "MapComponent.h"
#include "Entity.h"
#include "ProjectManager.h"
#include "World.h"

#include "trenchbroom/BakeLights.h"
#include "LevelFiles.h"
#include "trenchbroom/LightmapBaker.h"
#include "trenchbroom/MapMesh.h"

MapComponent::~MapComponent() = default;   // тело физики и состояние правки освобождают свои деструкторы

// --- Сериализация компонента ---

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

// --- Файлы ---

bool MapComponent::rejectIfEditing()
{
    if (!m_edit) return false;

    bakeStatus = "Exit edit mode first";
    return true;
}

bool MapComponent::createEmptyLevel(const std::filesystem::path& relativePath)
{
    if (rejectIfEditing()) return false;

    std::string error;
    if (!LevelFiles::CreateEmpty(relativePath, error)) { bakeStatus = error; return false; }

    return loadLevel(relativePath);   // выставит path и пустой меш
}

bool MapComponent::loadTrenchBroomMap(const std::filesystem::path& filepath)
{
    if (rejectIfEditing()) return false;
    return LevelFiles::IsLevelFile(filepath.string()) ? loadLevel(filepath) : importTrenchBroomMap(filepath, false);
}

bool MapComponent::importTrenchBroomMap(const std::filesystem::path& mapFile, bool overwrite)
{
    if (rejectIfEditing()) return false;

    std::filesystem::path levelPath;
    std::string error;
    if (!LevelFiles::Import(mapFile, overwrite, levelPath, error)) { bakeStatus = error; return false; }

    return loadLevel(levelPath);
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
    if (rejectIfEditing()) return false;

    LevelDocument loaded;
    bool migrated = false;
    std::string error;
    if (!LevelFiles::Load(filepath, loaded, migrated, error)) { bakeStatus = error; return false; }

    path = filepath.string();
    level = std::move(loaded);

    if (!rebuildFromDocument()) return false;
    if (migrated) m_levelDirty = true;   // уровень сохранится уже в новом формате

    return true;
}

bool MapComponent::saveLevel()
{
    if (!LevelFiles::IsLevelFile(path))
    {
        bakeStatus = "Nothing to save: no .cmap.json is loaded";
        return false;
    }

    std::string error;
    if (!LevelFiles::Save(path, level, error)) { bakeStatus = error; return false; }

    m_levelDirty = false;
    if (m_edit) m_edit->session().history().markSaved();

    return true;
}

std::filesystem::path MapComponent::lightmapCachePath() const
{
    return ProjectManager::Get().assetsPath() / (path + ".lightmap");
}

// --- Меш, физика, лайтмапа ---

bool MapComponent::rebuildFromDocument()
{
    return rebuildMesh() && m_physics.rebuild(level, owner, this);
}

bool MapComponent::rebuildMesh()
{
    if (level.solids.empty())
    {
        mesh.reset();
        lightmap.attach(nullptr, lightmapCachePath(), 1);
        return true;
    }

    auto newMesh = std::make_shared<MapMesh>();
    LightmapUnwrapSettings unwrap;
    unwrap.texelsPerUnit = lightmapTexelsPerUnit;
    if (!newMesh->buildFromLevel(level, unwrap)) { bakeStatus = "Map mesh build failed"; return false; }

    mesh = std::move(newMesh);
    bakeStatus.clear();
    lightmap.attach(mesh, lightmapCachePath(), unwrap.padding);
    return true;
}

bool MapComponent::rebuildLightmapUVs()
{
    if (level.solids.empty()) { bakeStatus = "Cannot rebuild lightmap UVs: no level is loaded"; return false; }
    if (!rebuildFromDocument()) return false;

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

    return lightmap.startBake(CollectBakeLights(*owner->scene));
}

bool MapComponent::isLightmapOutdated() const
{
    // FIXME: надо кешировать
    if (!mesh || !owner || !owner->scene) return false;
    return lightmap.isOutdated(CollectBakeLights(*owner->scene));
}

// --- Правка ---

bool MapComponent::beginEdit()
{
    if (m_edit) return true;
    if (isBaking()) { bakeStatus = "Cancel baking first"; return false; }
    if (owner && owner->scene && owner->scene->isSimulationMode()) return false;

    // Пустой компонент: заводим новый уровень рядом с остальными
    if (path == kNoMap && !createEmptyLevel(LevelFiles::NextUntitledPath())) return false;

    m_edit = std::make_unique<LevelEditState>(level);

    bakeStatus = "Editing geometry";
    return true;
}

void MapComponent::endEdit()
{
    if (!m_edit) return;

    if (m_edit->session().history().isDirty()) m_levelDirty = true;
    const bool geometryChanged = m_edit->changed();

    m_edit.reset();                // до пересборки: статика вернётся к запечённому MapMesh
    bakeStatus.clear();

    if (geometryChanged) rebuildFromDocument();
}

const ISurfaceGeometry* MapComponent::renderGeometry() const
{
    if (m_edit) return &m_edit->mesh();   // идёт правка
    return mesh.get();                    // запечённая геометрия (или nullptr)
}
