//
//  AudioComponent.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 09.09.26.
//

#include "AudioComponent.h"
#include "TransformComponent.h"
#include <cmath>

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

float AudioComponent::GetCurrentTime() const
{
    if (!m_isInitialized) return 0.0f;

    // 1. Получаем абсолютное время проигрывания
    ma_uint64 absoluteFrames = ma_sound_get_time_in_pcm_frames(&m_sound);
    ma_uint32 sampleRate = ma_engine_get_sample_rate(AudioSystem::Get().GetEngine());
    float absoluteTimeInSeconds = (float)absoluteFrames / (float)sampleRate;

    // 2. Получаем длину трека
    float totalLength = m_length;
    if (totalLength <= 0.0f) return 0.0f;

    // 3. Если включен лупинг, зацикливаем время математически
    if (IsLooping()) {
        return std::fmod(absoluteTimeInSeconds, totalLength);
    }

    // Если лупинг выключен, miniaudio останавливает трек на его максимальной длине
    return absoluteTimeInSeconds > totalLength ? totalLength : absoluteTimeInSeconds;
}