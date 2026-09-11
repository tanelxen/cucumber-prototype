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
    auto* transform = owner->getComponent<TransformComponent>();
    
    if (transform == nullptr) {
        m_camera.setTransform({0, 0, 0}, {0, 0, 1}, {1, 0, 0}, {0, 1, 0});
        return;
    }
    
    auto modelMatrix = transform->toMat4();
    
//    // Первые 3 столбца содержат базисные векторы (направление и масштаб)
//    glm::vec3 right   = glm::vec3(modelMatrix[0]); // Столбец 0: ось X (Right)
//    glm::vec3 up      = glm::vec3(modelMatrix[1]); // Столбец 1: ось Y (Up)
//    glm::vec3 forward = glm::vec3(modelMatrix[2]); // Столбец 2: ось Z (Forward/Backward)
//    
//    // Четвёртый столбец — это позиция (перемещение)
//    glm::vec3 position = glm::vec3(modelMatrix[3]);
//    
//    m_camera.setTransform(position, forward, right, up);
    
    m_camera.view = glm::inverse(modelMatrix);
}
