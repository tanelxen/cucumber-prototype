//
//  HitboxComponent.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 28.09.26.
//

#include "CollisionComponent.h"
#include "GizmoRender.h"
#include "Entity.h"

void CollisionComponent::save(nlohmann::json& j) const
{
    j = nlohmann::json {
        {"mins", {m_mins.x, m_mins.y, m_mins.z}},
        {"maxs", {m_maxs.x, m_maxs.y, m_maxs.z}},
        {"trigger", isTrigger},
        {"collisionMask", collisionMask},
        {"layer", layer}
    };
}

void CollisionComponent::load(const nlohmann::json& j)
{
    auto mins = j.at("mins").get<std::vector<float>>();
    m_mins = glm::vec3(mins[0], mins[1], mins[2]);
    
    auto maxs = j.at("maxs").get<std::vector<float>>();
    m_maxs = glm::vec3(maxs[0], maxs[1], maxs[2]);
    
    isTrigger = j.at("trigger").get<bool>();
    
    collisionMask = j.at("collisionMask").get<uint32_t>();
    layer = j.at("layer").get<uint32_t>();
}

void CollisionComponent::getWorldAABB(glm::vec3& outMins, glm::vec3& outMaxs)
{
    glm::vec3 worldPos = owner->getTransform().getWorldPosition();
    outMins = worldPos + m_mins;
    outMaxs = worldPos + m_maxs;
}

bool CollisionComponent::sweepTest(const glm::vec3& start, const glm::vec3& end,
                                   const glm::vec3& extMins, const glm::vec3& extMaxs,
                                   float& outFraction, glm::vec3& outNormal) const
{
    // 1. Мировые координаты цели
    glm::vec3 targetWorldPos = owner->getTransform().getWorldPosition();
    glm::vec3 targetMins = targetWorldPos + m_mins;
    glm::vec3 targetMaxs = targetWorldPos + m_maxs;
    
    // 2. Сумма Минковского (раздвигаем коробку цели на размеры летящей коробки)
    glm::vec3 boxMins = targetMins - extMaxs;
    glm::vec3 boxMaxs = targetMaxs - extMins;
    
    // Проверка на стартовое застревание (если мы уже внутри объекта)
    if (start.x >= boxMins.x && start.x <= boxMaxs.x &&
        start.y >= boxMins.y && start.y <= boxMaxs.y &&
        start.z >= boxMins.z && start.z <= boxMaxs.z)
    {
        outFraction = 0.0f;
        outNormal = glm::vec3(0.0f, 1.0f, 0.0f);
        return true;
    }
    
    glm::vec3 dir = end - start;
    float tNear = -1e30f; // Ближайшая точка пересечения (вход)
    float tFar  =  1e30f; // Дальнейшая точка пересечения (выход)
    
    glm::vec3 norm = glm::vec3(0.0f);
    
    for (int i = 0; i < 3; ++i) {
        if (std::abs(dir[i]) < 0.000001f) {
            // Луч параллелен оси: если мы вне границ коробки по этой оси, пересечения нет
            if (start[i] <= boxMins[i] || start[i] >= boxMaxs[i]) return false;
        } else {
            float ood = 1.0f / dir[i];
            float t1 = (boxMins[i] - start[i]) * ood;
            float t2 = (boxMaxs[i] - start[i]) * ood;
            
            // Определяем нормаль входа строго по направлению движения по оси:
            // Если летим вперед (dir > 0), врезаемся в ближнюю грань (нормаль -1)
            // Если летим назад (dir < 0), врезаемся в дальнюю грань (нормаль +1)
            glm::vec3 axisNormal = glm::vec3(0.0f);
            axisNormal[i] = (dir[i] > 0.0f) ? -1.0f : 1.0f;
            
            // Если t1 > t2, значит мы летим в отрицательном направлении.
            // t1 становится точкой ВЫХОДА, а t2 — точкой ВХОДА.
            if (t1 > t2) {
                std::swap(t1, t2);
            }
            
            // Нам нужно самое ПОЗДНЕЕ время входа среди всех трех осей
            if (t1 > tNear) {
                tNear = t1;
                norm = axisNormal; // Запоминаем нормаль той оси, которая ограничила вход
            }
            
            // Нам нужно самое РАННЕЕ время выхода среди всех трех осей
            if (t2 < tFar) {
                tFar = t2;
            }
            
            // Если вошли позже, чем вышли — луч пролетел мимо коробки
            if (tNear > tFar) return false;
        }
    }
    
    // 3. Если пересечение произошло в диапазоне движения луча
    if (tNear >= 0.0f && tNear < 1.0f) {
        
        // Внедряем фиксированный Quake-отступ (SURF_CLIP_EPSILON)
        // для предотвращения склеивания float-координат в следующем кадре
        const float SURF_CLIP_EPSILON = 0.125f;
        float pathLength = glm::length(dir);
        
        if (pathLength > 0.000001f) {
            tNear -= SURF_CLIP_EPSILON / pathLength;
            if (tNear < 0.0f) tNear = 0.0f;
        }
        
        outFraction = tNear;
        outNormal = norm;
        return true;
    }
    
    return false;
}


void CollisionComponent::gizmo()
{
    const glm::vec3& pos = owner->getTransform().getPosition();
    GizmoRender::Get().DrawAABB(pos + m_mins, pos + m_maxs, { 1, 1, 0, 1 });
}
