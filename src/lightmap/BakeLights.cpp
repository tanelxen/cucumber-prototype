//
//  BakeLights.cpp
//  Cucumber
//

#include "BakeLights.h"

#include "scene/Entity.h"
#include "scene/World.h"
#include "components/LightComponent.h"

std::vector<LightmapBakeLight> CollectBakeLights(const WorldScene& scene)
{
    std::vector<LightmapBakeLight> bakeLights;

    for (const auto& entity : scene.m_entities)
    {
        if (!entity || !entity->m_isActive) continue;

        const auto* component = entity->getComponent<LightComponent>();
        if (!component || !component->enabled
            || (component->mode != LightMode::Baked
                && component->mode != LightMode::BakedAndRealtime))
        {
            continue;
        }

        LightmapBakeLight light;
        light.type = static_cast<LightmapBakeLightType>(component->type);
        light.position = entity->getTransform().getWorldPosition();
        light.direction = entity->getTransform().getWorldRotation() * glm::vec3(0.0f, 0.0f, -1.0f);
        light.color = component->color;
        light.intensity = component->intensity;
        light.range = component->range;
        light.spotAngle = component->spotAngle;
        bakeLights.push_back(light);
    }

    return bakeLights;
}
