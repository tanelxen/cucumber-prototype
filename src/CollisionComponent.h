//
//  HitboxComponent.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 28.09.26.
//

#include "Component.h"
#include <glm/glm.hpp>

struct CollisionComponent : public Component
{
    const std::string getTypeName() const override { return "CollisionComponent"; }
    
    void save(nlohmann::json& j) const override;
    void load(const nlohmann::json& j) override;
    
    void gizmo() override;
    
    // Локальные границы объекта (относительно его Transform)
    glm::vec3 m_mins = { -15, 0, 15 };
    glm::vec3 m_maxs = { 15, 72, -15 };
    
    // --- СИСТЕМА ФИЛЬТРАЦИИ КОЛЛИЗИЙ ---
    // По умолчанию объект находится на 1-м слое и сталкивается со всеми слоями (0xFFFFFFFF)
    uint32_t layer = 1 << 0;
    uint32_t collisionMask = 0xFFFFFFFF; // Не используется
    
    bool isTrigger = false;
    
    std::function<void(Entity*)> onTriggerEnter;
    std::function<void(Entity*)> onTriggerExit;
    
    // Хелпер получения мирового AABB (учитывая позицию сущности)
    void getWorldAABB(glm::vec3& outMins, glm::vec3& outMaxs);
    
    // Сюда мы добавим метод математической проверки пересечения (см. Шаг 2)
    bool sweepTest(const glm::vec3& start, const glm::vec3& end,
                   const glm::vec3& extMins, const glm::vec3& extMaxs,
                   float& outFraction, glm::vec3& outNormal) const;
    
    void CallTriggerEnter(Entity* other)
    {
        if (onTriggerEnter)
        {
            onTriggerEnter(other);
        }
    }
};
