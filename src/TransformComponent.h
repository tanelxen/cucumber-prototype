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
    ~TransformComponent();
    
    glm::vec3 position = { 0.0f, 0.0f, 0.0f };
    glm::vec3 rotation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
    
    glm::mat4 toMat4();
    
    const std::string getTypeName() const override { return "TransformComponent"; }
    
    void save(nlohmann::json& j) const override
    {
        j = nlohmann::json {
            {"position", {position.x, position.y, position.z}},
            {"rotation", {rotation.x, rotation.y, rotation.z}},
            {"scale",    {scale.x,    scale.y,    scale.z}}
        };
    }
    
    void load(const nlohmann::json& j) override
    {
        auto pos = j.at("position").get<std::vector<float>>();
        position = glm::vec3(pos[0], pos[1], pos[2]);
        
        auto rot = j.at("rotation").get<std::vector<float>>();
        rotation = glm::vec3(rot[0], rot[1], rot[2]);
        
        auto scl = j.at("scale").get<std::vector<float>>();
        scale = glm::vec3(scl[0], scl[1], scl[2]);
    }
};
