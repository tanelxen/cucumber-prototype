//
//  TrenchBroomMapComponent.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 21.10.26.
//

#include "MapComponent.h"
#include "ProjectManager.h"
#include "PhysicsSystem.h"

#include "trenchbroom/MapAsset.h"
#include "trenchbroom/MapMesh.h"

#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include <iostream>
#include <cmath>

static inline glm::vec3 Quake2OpenGL(const glm::vec3& in)
{
    return { in.x, in.z, -in.y };
}

// Пересечение трех плоскостей для генерации вершин браша
static bool GetMapPlaneIntersection(const MapPlane& p1, const MapPlane& p2, const MapPlane& p3, glm::vec3& outPoint)
{
    glm::vec3 cross12 = glm::cross(p1.normal, p2.normal);
    float denom = glm::dot(cross12, p3.normal);
    
    if (std::abs(denom) < 1e-4f) return false;
    
    outPoint = (glm::cross(p2.normal, p3.normal) * p1.distance +
                glm::cross(p3.normal, p1.normal) * p2.distance +
                cross12 * p3.distance) / denom;
    return true;
}

// Проверка: находится ли сгенерированная вершина внутри выпуклого объема браша
static bool IsPointInsideMapBrush(const glm::vec3& point, const MapBrush& brush)
{
    for (const auto& side : brush.sides)
    {
        // В стандартных .map файлах нормали смотрят СНАРУЖИ браша.
        // Точка внутри, если проекция на нормаль меньше или равна дистанции плоскости (+ погрешность)
        if (glm::dot(side.plane.normal, point) - side.plane.distance > 0.005f) {
            return false;
        }
    }
    return true;
}

bool MapComponent::loadTrenchBroomMap(const std::filesystem::path& filepath)
{
    path = filepath.string();
    
    MapAsset mapData;
    if (!mapData.loadFromFile(ProjectManager::Get().assetsPath() / filepath.string())) {
        std::cout << "[Map Error] Cannot open or parse .map file: " << filepath << std::endl;
        return false;
    }
    
    // Инициализируем графический меш
    mesh = std::make_shared<MapMesh>();
    mesh->buildFromMapAsset(mapData);
    
    // --- ИНТЕГРАЦИЯ С JOLT ЧЕРЕЗ БРАШИ ---
    destroyPhysicsBody();
    
    JPH::StaticCompoundShapeSettings compoundSettings;
    JPH::Array<JPH::Vec3> brushVertices;
    brushVertices.reserve(64);
    
    // В файлах TrenchBroom первый энтити (индекс 0) — это всегда "worldspawn"
    // (вся статическая архитектура уровня: стены, пол, потолок)
    if (mapData.entities.empty()) return false;
    const auto& worldspawn = mapData.entities[0];
    
    for (const auto& brush : worldspawn.brushes)
    {
        if (brush.sides.size() < 4) continue;
        
        brushVertices.clear();
        size_t numSides = brush.sides.size();
        
        // Пересекаем каждую тройку плоскостей браша для восстановления полигонального представления
        for (size_t i = 0; i < numSides; ++i) {
            for (size_t j = i + 1; j < numSides; ++j) {
                for (size_t k = j + 1; k < numSides; ++k) {
                    
                    glm::vec3 intersectionPoint;
                    if (GetMapPlaneIntersection(brush.sides[i].plane, brush.sides[j].plane, brush.sides[k].plane, intersectionPoint))
                    {
                        if (IsPointInsideMapBrush(intersectionPoint, brush))
                        {
                            // Конвертируем систему координат Quake -> OpenGL -> Jolt
                            glm::vec3 openGLPos = Quake2OpenGL(intersectionPoint);
                            JPH::Vec3 joltPos = QuakeToJolt(openGLPos);
                            
                            // Избавляемся от дубликатов на стыках плоскостей
                            bool duplicate = false;
                            for (const auto& v : brushVertices) {
                                if (v.IsClose(joltPos, QUAKE_UNITS_TO_METERS)) {
                                    duplicate = true;
                                    break;
                                }
                            }
                            
                            if (!duplicate) {
                                brushVertices.push_back(joltPos);
                            }
                        }
                    }
                    
                }
            }
        }
        
        // Если получился валидный 3D-объем, скармливаем его в физику Jolt
        if (brushVertices.size() >= 4)
        {
            JPH::ConvexHullShapeSettings convexSettings(brushVertices);
            convexSettings.mMaxConvexRadius = 0.01f;
            
            JPH::Shape::ShapeResult shapeResult = convexSettings.Create();
            if (shapeResult.IsValid()) {
                compoundSettings.AddShape(JPH::Vec3::sZero(), JPH::Quat::sIdentity(), shapeResult.Get());
            }
        }
    }
    
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
}

void MapComponent::load(const nlohmann::json& j)
{
    if (j.contains("source_map"))
    {
        path = j.at("source_map");
    }
}

void MapComponent::reload()
{
    if (path != "<no map>")
    {
        loadTrenchBroomMap(path);
    }
}
