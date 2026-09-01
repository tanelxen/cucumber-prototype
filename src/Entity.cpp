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

void AudioComponent::update(float dt)
{
    auto comp = owner->getComponent<TransformComponent>();
    auto transform = static_cast<TransformComponent*>(comp);
    
    ma_sound_set_spatialization_enabled(&m_sound, true);
    ma_sound_set_positioning(&m_sound, ma_positioning_absolute);
    ma_sound_set_attenuation_model(&m_sound, ma_attenuation_model_linear);
    
    ma_sound_set_rolloff(&m_sound, 1.0f);
    ma_sound_set_min_distance(&m_sound, 80.0f);
    ma_sound_set_max_distance(&m_sound, 1250.0f);
    
    ma_sound_set_position(&m_sound, transform->position.x, transform->position.y, transform->position.z);
}
