//
//  MovementComponent.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 20.09.26.
//

// MovementComponent.h
#pragma once

#include "Component.h"
#include "PhysicsCommon.h"
#include <glm/glm.hpp>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

#include "../vendor/json.hpp"

class MovementComponent : public Component
{
public:
    MovementComponent();
    ~MovementComponent() override;
    
    void awake() override;
    void update(float dt) override;
    void gizmo() override;
    
    void save(nlohmann::json& j) const override { j["forward_speed"] = m_forwardspeed; }
    void load(const nlohmann::json& j) override { m_forwardspeed = j.value("forward_speed", 400.f); }
    
    const std::string getTypeName() const override { return "MovementComponent"; }
    
    void addMovementInput(const glm::vec3& direction, float scale = 1.0f);
    void jump();
    void launch(const glm::vec3& launchVelocity, bool overrideCurrent);
    
    const glm::vec3& getVelocity() const { return m_velocity; }
    bool isWalking() const { return m_isGrounded && glm::length(m_velocity) > 0.1f; }
    bool isGrounded() const { return m_isGrounded; }
    
    float m_forwardspeed = 400.f;
    
private:
    void HandleGroundAndFriction(float dt);
    void HandleMovementAndAcceleration(float dt);
    
private:
    // Ссылка на виртуального персонажа Jolt
    JPH::CharacterVirtual* m_character = nullptr;
    PhysicsUserData m_userData;
    
    glm::vec3 m_inputVector{0.0f};
    glm::vec3 m_velocity{0.0f};
    
    bool m_isGrounded = false;
    bool m_wantJump = false;
};
