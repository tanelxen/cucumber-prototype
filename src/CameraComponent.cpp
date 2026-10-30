//
//  CameraComponent.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 12.09.26.
//

#include "CameraComponent.h"
#include "TransformComponent.h"
#include "Entity.h"

#include "GizmoRender.h"

CameraComponent::CameraComponent() : Component(Flags::RequiresUpdate)
{
    
}

void CameraComponent::gizmo()
{
    const glm::mat4& worldMatrix = owner->getTransform().getWorldMatrix();
    glm::vec3 worldPos = glm::vec3(worldMatrix[3]);
    
    GizmoRender::Get().DrawCamera(worldMatrix, 32.f, 32.f, {0, 1, 0, 1});
}

void CameraComponent::save(nlohmann::json& j) const
{
    j["enabled"] = enabled;
    j["primary"] = m_isPrimary;
    j["overlay"] = m_isOverlay;
    j["cameraStackId"] = cameraStackId;
    
    j["clearDepthOnly"] = clearDepthOnly;
    j["cullingMask"] = cullingMask;
    j["depth"] = depth;
    
    j["fov"] = fov;
}

void CameraComponent::load(const nlohmann::json& j)
{
    if (j.contains("enabled"))
    {
        enabled = j.at("enabled");
    }
    else if (j.contains("current"))
    {
        enabled = j.at("current");
    }
    
    if(j.contains("clearDepthOnly"))
    {
        clearDepthOnly = j.at("clearDepthOnly");
    }
    
    if (j.contains("overlay"))
    {
        m_isOverlay = j.at("overlay");
    }
    else if (j.contains("current"))
    {
        m_isOverlay = clearDepthOnly;
    }
    
    if (j.contains("primary"))
    {
        m_isPrimary = j.at("primary");
    }
    
    if (j.contains("cameraStackId"))
    {
        cameraStackId = j.at("cameraStackId");
    }
    
    if(j.contains("cullingMask"))
    {
        cullingMask = j.at("cullingMask");
    }
    
    if(j.contains("depth"))
    {
        depth = j.at("depth");
    }
    
    if(j.contains("fov"))
    {
        fov = j.value("fov", 55.f);
    }
}
