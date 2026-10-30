//
//  RigidbodyComponent.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 18.10.26.
//

#pragma once

#include "Component.h"
#include "PhysicsCommon.h"
#include <glm/glm.hpp>
#include <Jolt/Physics/Body/BodyID.h>

enum class PhysicsShapeType {
    Box,
    Sphere
};

enum class PhysicsMotionType {
    Static,    // Для неподвижных платформ
    Dynamic,   // Для падающих кубов, коробок
    Kinematic  // Для двигающихся скриптами платформ
};

struct RigidbodyComponent : public Component
{
    const std::string getTypeName() const override { return "RigidbodyComponent"; }
    
    void awake() override {
        CreatePhysicsBody();
    }
    
    ~RigidbodyComponent() {
        DestroyPhysicsBody();
    }
    
    // Настройки формы и движения (можно менять перед стартом сцены)
    PhysicsShapeType shapeType = PhysicsShapeType::Box;
    PhysicsMotionType motionType = PhysicsMotionType::Dynamic;
    
    // Размеры (для Box — это half-extents, то есть половина размера по осям)
    glm::vec3 boxHalfExtents = { 16.0f, 16.0f, 16.0f };
    float sphereRadius = 1.0f;
    
    float mass = 1.0f;
    float friction = 0.5f;
    float restitution = 0.2f; // Упругость (попрыгучесть)
    
    void addImpulse(const glm::vec3& force, const glm::vec3& point);
    
    void save(nlohmann::json& j) const override;
    void load(const nlohmann::json& j) override;
    
    void gizmo() override;
    
    // Jolt-специфичные данные
    JPH::BodyID m_bodyID;
    PhysicsUserData m_userData;
    
    void CreatePhysicsBody();
    void DestroyPhysicsBody();
    void SyncTransformToPhysics();
    void SyncPhysicsToTransform();
};

