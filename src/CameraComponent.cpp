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
