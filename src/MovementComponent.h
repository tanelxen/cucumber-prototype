//
//  MovementComponent.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 20.09.26.
//

#pragma once

#include "Component.h"
#include <glm/glm.hpp>

#include "../vendor/json.hpp"

class MovementComponent : public Component
{
public:
    MovementComponent();
    
    void save(nlohmann::json& j) const override
    {
        
    }
    
    void load(const nlohmann::json& j) override
    {
        
    }
    
    const std::string getTypeName() const override { return "MovementComponent"; }
    
    void update(float dt) override;
    
    void addMovementInput(const glm::vec3& direction, float scale = 1.0f);
    void jump();
    
    // Если надо перезаписать скорость
    void launch(const glm::vec3& launchVelocity, bool overrideCurrent);
    
    const glm::vec3& getVelocity() const { return m_velocity; }
    bool isWalking() const { return m_isGrounded && glm::length(m_velocity) > 0.1f; }
    int getSurfaceFlags() const { return m_surfaceFlags; }
    
private:
    void apply_inputs(float dt);
    void apply_air_control(const glm::vec3& direction, float wishspeed, float dt);
    void apply_acceleration(const glm::vec3& direction, float wishspeed, float acceleration, float dt);
    void apply_friction(float dt);
    void apply_jump();
    
    void trace_ground();
    void step_slide(bool gravity, float dt);
    bool slide(bool gravity, float dt);
    
private:
    glm::vec3 m_inputVector{0.0f};
    
    // Физическое состояние
    glm::vec3 m_position;
    glm::vec3 m_forward;
    glm::vec3 m_right;
    
    glm::vec3 m_velocity{0.0f};
    glm::vec3 m_groundNormal{0.0f};
    int m_surfaceFlags = 0;
    bool m_isGrounded = false;
    int m_movementBits = 0;
};

