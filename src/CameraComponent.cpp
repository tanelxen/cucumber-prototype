//
//  CameraComponent.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 12.09.26.
//

#include "CameraComponent.h"
#include "TransformComponent.h"
#include "Entity.h"

CameraComponent::CameraComponent() : Component(Flags::RequiresUpdate)
{
    
}

void CameraComponent::update(float dt)
{
    auto modelMatrix = owner->getTransform().getWorldMatrix();
    m_camera.view = glm::inverse(modelMatrix);
}
