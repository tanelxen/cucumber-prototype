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

Entity::Entity(const std::string &name) : m_name(name)
{
}

//template<typename T, typename... Args>
//void Entity::addComponent(Args&&... args)
//{
//    m_components[typeid(T)] = std::make_unique<T>(std::forward<Args>(args)...);
//}

//template<typename T>
//T* Entity::getComponent()
//{
//    auto it = m_components.find(typeid(T));
//    if (it != m_components.end()) {
//        return static_cast<T*>(it->second.get());
//    }
//    return nullptr;
//}

glm::mat4 Transform::toMat4()
{
    
//    glm::mat4 t = glm::mat4(1.0f);
//    t = glm::translate(glm::mat4(1.0f), glm::vec3(position[0], position[1], position[2]));
//    t *= glm::toMat4(glm::quat(rotation[3], rotation[0], rotation[1], rotation[2]));
//    t *= glm::scale(glm::mat4(1.0f), glm::vec3(scale[0], scale[1], scale[2]));
    
//    glm::quat rot = glm::quat
    
//    glm::eulerAngleXYZ(rotation.x, rotation.y, rotation.z);
    
    
    glm::mat4 model(1.0f);
    model = glm::translate(model, position);
//    model = glm::rotate(model, float(radians(rotation.y)), glm::vec3(0, 1, 0));
//    model = glm::rotate(model, inst->pitch, glm::vec3(0, 1, 0));
    model = glm::scale(model, scale);
    
    return model;
}

glm::mat4 TransformComponent::toMat4()
{
    glm::mat4 model(1.0f);
    model = glm::translate(model, position);
//    model = glm::rotate(model, float(radians(rotation.y)), glm::vec3(0, 1, 0));
    
    model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1, 0, 0));
    model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0, 1, 0));
    model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0, 0, 1));
    
    model = glm::scale(model, scale);
    
    return model;
}
