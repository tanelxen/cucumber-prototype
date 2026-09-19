//
//  TransformComponent.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 10.09.26.
//

#pragma once

#include "Component.h"
#include <glm/glm.hpp>

struct TransformComponent : public Component
{
    glm::vec3 position = { 0.0f, 0.0f, 0.0f };
    glm::vec3 rotation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
    
    glm::mat4 toMat4();
    
    glm::vec3 getForward();
    glm::vec3 getRight();
    
    const std::string getTypeName() const override { return "TransformComponent"; }
    
    void save(nlohmann::json& j) const override;
    void load(const nlohmann::json& j) override;
    
private:
    glm::mat4 getLocalMat4() const;
    
    glm::mat4 m_worldMatrix;
    bool m_isDirty = true;
};
