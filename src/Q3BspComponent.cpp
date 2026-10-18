//
//  BSPMeshComponent.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 16.10.26.
//

#include "Q3BspComponent.h"

#include "q3/Q3BSPAsset.h"
#include "q3/Q3BspMesh.h"

#include "PhysicsSystem.h"
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

#include <iostream>
#include <vector>

static inline glm::vec3 Quake2OpenGL(const glm::vec3& in)
{
    return { in.x, in.z, -in.y };
}

// Функция нахождения точки пересечения трех плоскостей
static bool GetPlaneIntersection(const tBSPPlane& p1, const tBSPPlane& p2, const tBSPPlane& p3, glm::vec3& outPoint)
{
    // Матрица из нормалей плоскостей
    glm::vec3 cross12 = glm::cross(p1.normal, p2.normal);
    float denom = glm::dot(cross12, p3.normal);
    
    // Если определитель близок к нулю, плоскости параллельны или пересекаются по линии
    if (std::abs(denom) < 1e-4f) return false;
    
    outPoint = (glm::cross(p2.normal, p3.normal) * p1.distance +
                glm::cross(p3.normal, p1.normal) * p2.distance +
                cross12 * p3.distance) / denom;
    return true;
}

// Функция проверки, находится ли точка внутри всех плоскостей браша
static bool IsPointInsideBrush(const glm::vec3& point, const Q3BSPAsset& bsp, const tBSPBrush& brush)
{
    for (int i = 0; i < brush.numOfBrushSides; ++i) {
        const tBSPBrushSide& side = bsp.m_brushSides[brush.brushSide + i];
        const tBSPPlane& plane = bsp.m_planes[side.plane];
        
        // В Quake 3: если dot(normal, point) - distance > 0, точка находится "снаружи" плоскости браша
        if (glm::dot(plane.normal, point) - plane.distance > 0.05f) {
            return false;
        }
    }
    return true;
}

bool Q3BspComponent::loadQ3BSP(const std::filesystem::path& filepath)
{
    Q3BSPAsset bsp;
    
    if (!bsp.initFromFile(filepath.string().c_str())) {
        std::cout << "[Physics Error] Cannot open or parse BSP file: " << filepath << std::endl;
        return false;
    }
    
    // Инициализируем графический меш
    mesh = std::make_shared<Q3BspMesh>();
    mesh->initFromBsp(&bsp);
    
    // --- ИНТЕГРАЦИЯ С JOLT ЧЕРЕЗ БРАШИ ---
    
    // Создаем настройки для составной статичной формы уровня
    JPH::StaticCompoundShapeSettings compoundSettings;
    JPH::Array<JPH::Vec3> brushVertices;
    brushVertices.reserve(100);
    
    // В формате Quake 3 BSP модель под индексом 0 — это ВСЕГДА "worldspawn" (сама статичная карта).
    // Все остальные модели (1, 2, 3...) — это динамические объекты: двери, лифты и ТРИГГЕРЫ.
    const tBSPModel& worldModel = bsp.m_models[0];
    
    // Перебираем все браши в BSP файле
    for (int b = 0; b < worldModel.numOfBrushes; ++b)
    {
        int brushIndex = worldModel.brushIndex + b;
        const tBSPBrush& brush = bsp.m_brushes[brushIndex];
        
        // Какой-то вырожденый браш
        if (brush.numOfBrushSides < 4) continue;
        
        int contents = bsp.m_textures[brush.textureID].contents;
        
        // Отсекаем жидкости, туман и не-солидную геометрию, которая может быть в worldspawn
        if (contents & 0x40000000) continue; // На всякий случай оставляем маску CONTENTS_TRIGGER
        if (contents & (0x2 | 0x4 | 0x8 | 0x40)) continue; // Вода, лава, туман
        
        bool isSolid = (contents & 1);
        if (!isSolid) continue;
        
        brushVertices.clear();
        
        // Находим вершины браша путем пересечения каждых трех его плоскостей
        for (int i = 0; i < brush.numOfBrushSides; ++i)
        {
            for (int j = i + 1; j < brush.numOfBrushSides; ++j)
            {
                for (int k = j + 1; k < brush.numOfBrushSides; ++k)
                {
                    const tBSPBrushSide& side1 = bsp.m_brushSides[brush.brushSide + i];
                    const tBSPBrushSide& side2 = bsp.m_brushSides[brush.brushSide + j];
                    const tBSPBrushSide& side3 = bsp.m_brushSides[brush.brushSide + k];
                    
                    const tBSPPlane& p1 = bsp.m_planes[side1.plane];
                    const tBSPPlane& p2 = bsp.m_planes[side2.plane];
                    const tBSPPlane& p3 = bsp.m_planes[side3.plane];
                    
                    glm::vec3 intersectionPoint;
                    
                    if (GetPlaneIntersection(p1, p2, p3, intersectionPoint))
                    {
                        // Если точка лежит внутри всех остальных полупространств браша, это реальная вершина
                        if (IsPointInsideBrush(intersectionPoint, bsp, brush))
                        {
                            glm::vec3 openGLPos = Quake2OpenGL(intersectionPoint);
                            
                            // Защита от дублирования точек (улучшает стабильность ConvexHullShape в Jolt)
                            JPH::Vec3 joltPos(openGLPos.x, openGLPos.y, openGLPos.z);
                            bool duplicate = false;
                            
                            for (const auto& v : brushVertices)
                            {
                                if (v.IsClose(joltPos, 1.0f))
                                {
                                    duplicate = true;
                                    break;
                                }
                            }
                            
                            if (!duplicate)
                            {
                                brushVertices.push_back(joltPos);
                            }
                        }
                    }
                }
            }
        }
        
        // Если вершины успешно найдены, создаем ConvexHullShape
        if (brushVertices.size() >= 4)
        {
            JPH::ConvexHullShapeSettings convexSettings(brushVertices);
            
            // Задаем внутренний радиус скругления (опционально, предотвращает застревания)
            convexSettings.mMaxConvexRadius = 0.01f;
            
            JPH::Shape::ShapeResult shapeResult = convexSettings.Create();
            
            if (shapeResult.IsValid())
            {
                compoundSettings.AddShape(JPH::Vec3::sZero(), JPH::Quat::sIdentity(), shapeResult.Get());
            }
        }
    }
    
    // Создаем финальную общую форму уровня
    JPH::Shape::ShapeResult shapeResult = compoundSettings.Create();
    
    if (shapeResult.HasError())
    {
        std::cout << shapeResult.GetError() << std::endl;
        return false;
    }
    
    // Получаем ссылку на интерфейс управления телами из синглтона нашей физической системы
    auto& bodyInterface = PhysicsSystem::Get().GetBodyInterface();
    
    JPH::BodyCreationSettings settings;
    settings.SetShape(shapeResult.Get());
    settings.mMotionType = JPH::EMotionType::Static;
    settings.mObjectLayer = Layers::STATIC;
    settings.mFriction = 0.2;
    
    // Создаем тело и связываем UserData
    JPH::Body* body = bodyInterface.CreateBody(settings);
    m_bodyID = body->GetID();
        
    // Привязываем UserData к телу перед добавлением в мир,
    // чтобы Jolt сразу проинициализировал внутренние указатели
    m_userData.entity = owner;
    m_userData.component = this;
    bodyInterface.SetUserData(m_bodyID, reinterpret_cast<JPH::uint64>(&m_userData));
    
    // Добавляем созданное тело в симуляцию физического мира.
    // Так как это статический уровень, активация ему не нужна (DontActivate)
    bodyInterface.AddBody(m_bodyID, JPH::EActivation::DontActivate);
    
    if (m_bodyID.IsInvalid())
    {
        std::cout << "[Physics Error] BSP Body was NOT created!" << std::endl;
        return false;
    }
    
    std::cout << "[Physics] Successfully created JPH::Body via " << compoundSettings.mSubShapes.size() << " convex brushes" << std::endl;
    return true;
}

void Q3BspComponent::destroyPhysicsBody()
{
    if (!m_bodyID.IsInvalid())
    {
        auto& bodyInterface = PhysicsSystem::Get().GetBodyInterface();
        bodyInterface.RemoveBody(m_bodyID);
        bodyInterface.DestroyBody(m_bodyID);
        m_bodyID = JPH::BodyID();
    }
}

Q3BspComponent::~Q3BspComponent()
{
    destroyPhysicsBody();
}

void Q3BspComponent::save(nlohmann::json& j) const
{
    j["source_bsp"] = path;
}

void Q3BspComponent::load(const nlohmann::json& j)
{
    if(j.contains("source_bsp"))
    {
        path = j.at("source_bsp");
    }
}

//static bool intersection(const tBSPPlane& p1, const tBSPPlane& p2, const tBSPPlane& p3, glm::vec3& vertex)
//{
//    glm::vec3 n1 = p1.normal;
//    glm::vec3 n2 = p2.normal;
//    glm::vec3 n3 = p3.normal;
//
//    float d1 = p1.distance;
//    float d2 = p2.distance;
//    float d3 = p3.distance;
//
//    float denom = glm::dot(n1, glm::cross(n2, n3));
//
//    if (denom <= 1e-5 && denom >= -1e-5) { return false; }
//
//    vertex = d1 * glm::cross(n2, n3) + d2 * glm::cross(n3, n1) + d3 * glm::cross(n1, n2);
//    vertex /= denom;
//
//    return true;
//}

//static bool isPointInsideVolume(const glm::vec3& vertex, const std::vector<tBSPPlane>& planes)
//{
//    for (const auto& plane : planes)
//    {
//        float distanceToPlane = glm::dot(plane.normal, vertex) - plane.distance;
//
//        if (distanceToPlane > 1e-5)
//        {
//            return false;
//        }
//    }
//
//    return true;
//}
