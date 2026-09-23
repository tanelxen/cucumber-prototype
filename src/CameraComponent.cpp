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

void CameraComponent::update(float dt)
{
    auto modelMatrix = owner->getTransform().getWorldMatrix();
    m_camera.view = glm::inverse(modelMatrix);
}

void CameraComponent::gizmo()
{
    const glm::mat4& worldMatrix = owner->getTransform().getWorldMatrix();
    glm::vec3 worldPos = glm::vec3(worldMatrix[3]);
    
    GizmoRender::Get().DrawSphere(worldPos, 16.0f, {0, 1, 0, 1});
    
//    GizmoRender::Get().DrawIcon3D(worldPos, {16, 16}, {0, 1, 0, 1});
//    GizmoRender::Get().DrawText3D(worldPos, "Camera", 16, {0, 1, 0, 1});
}
