//
//  LightComponent.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 22.10.26.
//

#include "LightComponent.h"

void LightComponent::save(nlohmann::json& j) const
{
    j["type"] = (int)type;
    j["mode"] = static_cast<int>(mode);
    j["color"] = {color.x, color.y, color.z};
    j["intensity"] = intensity;
    j["range"] = range;
    j["spotAngle"] = spotAngle;
    j["castShadows"] = castShadows;
}

void LightComponent::load(const nlohmann::json& j)
{
    if(j.contains("type")) type = (LightType)j.at("type");
    if (j.contains("mode"))
    {
        const int modeValue = j.at("mode").get<int>();
        if (modeValue >= static_cast<int>(LightMode::Realtime)
            && modeValue <= static_cast<int>(LightMode::BakedAndRealtime))
        {
            mode = static_cast<LightMode>(modeValue);
        }
    }
    if(j.contains("color")) {
        auto c = j.at("color");
        color = glm::vec3(c[0], c[1], c[2]);
    }
    if(j.contains("intensity")) intensity = j.at("intensity");
    if(j.contains("range")) range = j.at("range");
    if(j.contains("spotAngle")) spotAngle = j.at("spotAngle");
    if(j.contains("castShadows")) castShadows = j.at("castShadows");
}
