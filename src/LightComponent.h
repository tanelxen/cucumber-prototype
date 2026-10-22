//
//  LightComponent.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 22.10.26.
//

#pragma once

#include "Component.h"
#include <glm/glm.hpp>

enum class LightType { Directional, Point, Spot };
enum class LightMode { Realtime, Baked, BakedAndRealtime };

struct LightComponent : public Component
{
    LightType type = LightType::Point;
    LightMode mode = LightMode::Realtime;
    
    glm::vec3 color     = glm::vec3(1.0f, 0.7f, 0.3f);
    float intensity     = 8.0f;
    
    // Параметры затухания для PointLight
    float range         = 512.0f;
    float linear        = 0.09f;
    float quadratic     = 0.032f;
    float spotAngle     = 30.0f;
    bool castShadows    = true;
    
    const std::string getTypeName() const override { return "LightComponent"; }
    
    void save(nlohmann::json& j) const override;
    void load(const nlohmann::json& j) override;
};
