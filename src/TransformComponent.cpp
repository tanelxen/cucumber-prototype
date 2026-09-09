//
//  TransformComponent.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 10.09.26.
//

#include "TransformComponent.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

TransformComponent::~TransformComponent()
{
    std::cout << "~TransformComponent(): " << std::endl;
}

glm::mat4 TransformComponent::toMat4()
{
    glm::mat4 model(1.0f);
    
    model = glm::translate(model, position);
    
    model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1, 0, 0));
    model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0, 1, 0));
    model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0, 0, 1));
    
    model = glm::scale(model, scale);
    
    return model;
}
