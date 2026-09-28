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
    
    void save(nlohmann::json& j) const override
    {
        j = nlohmann::json {
            {"mins", {m_mins.x, m_mins.y, m_mins.z}},
            {"maxs", {m_maxs.x, m_maxs.y, m_maxs.z}},
            {"collisionMask", collisionMask},
            {"layer", layer}
        };
    }
    
    void load(const nlohmann::json& j) override
    {
        auto mins = j.at("mins").get<std::vector<float>>();
        m_mins = glm::vec3(mins[0], mins[1], mins[2]);
        
        auto maxs = j.at("maxs").get<std::vector<float>>();
        m_maxs = glm::vec3(maxs[0], maxs[1], maxs[2]);
        
        collisionMask = j.at("collisionMask").get<uint32_t>();
        layer = j.at("layer").get<uint32_t>();
    }
    
    void gizmo() override;
    
    // Локальные границы объекта (относительно его Transform)
    glm::vec3 m_mins = { -15, 0, 15 };
    glm::vec3 m_maxs = { 15, 72, -15 };
    
    // --- СИСТЕМА ФИЛЬТРАЦИИ КОЛЛИЗИЙ ---
    // По умолчанию объект находится на 1-м слое и сталкивается со всеми слоями (0xFFFFFFFF)
    uint32_t layer = 1 << 0;
    uint32_t collisionMask = 0xFFFFFFFF;
    
    // Хелпер получения мирового AABB (учитывая позицию сущности)
    void getWorldAABB(glm::vec3& outMins, glm::vec3& outMaxs);
    
    // Сюда мы добавим метод математической проверки пересечения (см. Шаг 2)
    bool sweepTest(const glm::vec3& start, const glm::vec3& end,
                   const glm::vec3& extMins, const glm::vec3& extMaxs,
                   float& outFraction, glm::vec3& outNormal) const;
};
