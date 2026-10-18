//
//  HitboxComponent.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 28.09.26.
//

#include "Component.h"
#include "PhysicsCommon.h"
#include <glm/glm.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

struct CollisionComponent : public Component
{
    ~CollisionComponent();
    
    const std::string getTypeName() const override { return "CollisionComponent"; }
    
    void awake() override;
    
    void save(nlohmann::json& j) const override;
    void load(const nlohmann::json& j) override;
    
    void gizmo() override;
    
    // Локальные границы объекта (относительно его Transform)
    glm::vec3 m_mins = { -16, -16, -16 };
    glm::vec3 m_maxs = { 16, 16, 16 };
    
    // --- СИСТЕМА ФИЛЬТРАЦИИ КОЛЛИЗИЙ ---
    // По умолчанию объект находится на 1-м слое и сталкивается со всеми слоями (0xFFFFFFFF)
    uint32_t layer = 1 << 0;
    uint32_t collisionMask = 0xFFFFFFFF; // Не используется
    
    bool isTrigger = false;
    
    std::function<void(Entity*)> onTriggerEnter;
    std::function<void(Entity*)> onTriggerExit;
    
    void CallTriggerEnter(Entity* other)
    {
        if (onTriggerEnter)
        {
            onTriggerEnter(other);
        }
    }
    
    
    // Ссылка на тело в Jolt
    JPH::BodyID m_bodyID;
    PhysicsUserData m_userData;
    
    void CreatePhysicsBody();
    void DestroyPhysicsBody();
    void SyncTransformToPhysics();
    void SyncPhysicsToTransform();
};
