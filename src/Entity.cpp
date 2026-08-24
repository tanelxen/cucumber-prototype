//
//  SourceMapScene.cpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 12.05.25.
//

#include "Entity.h"
//#include "glm/gtc/quaternion.hpp"
//#include <glm/gtx/euler_angles.hpp>
#include "glm/gtc/matrix_transform.hpp"

//#define radians(deg) ((deg) * (M_PI / 180.0f))

Entity::Entity(const std::string &name) : m_name(name), m_isActive(true)
{
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
