//
//  AudioComponent.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 09.09.26.
//

#include "AudioComponent.h"
#include "TransformComponent.h"

AudioComponent::AudioComponent() : Component(Flags::RequiresUpdate)
{
    
}

void AudioComponent::update(float dt)
{
//    auto transform = owner->getComponent<TransformComponent>();
    
//    if (transform)
//    {
//        ma_sound_set_spatialization_enabled(&m_sound, true);
//        ma_sound_set_positioning(&m_sound, ma_positioning_absolute);
//        ma_sound_set_attenuation_model(&m_sound, ma_attenuation_model_linear);
//
//        ma_sound_set_rolloff(&m_sound, 1.0f);
//        ma_sound_set_min_distance(&m_sound, 80.0f);
//        ma_sound_set_max_distance(&m_sound, 1250.0f);
//
//        ma_sound_set_position(&m_sound, transform->position.x, transform->position.y, transform->position.z);
//    }
}
