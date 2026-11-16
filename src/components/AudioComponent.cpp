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

void AudioComponent::save(nlohmann::json& j) const
{
    j = nlohmann::json{ {"source", path} };
}

void AudioComponent::load(const nlohmann::json& j)
{
    path = j.at("source");
}

void AudioComponent::init(const std::string& filePath)
{
    destroy(); // Очищаем старый звук, если он был
    
    ma_uint32 flags = 0;
    
    ma_result result = ma_sound_init_from_file(AudioSystem::Get().GetEngine(),
                                               filePath.c_str(),
                                               flags,
                                               nullptr, // Группа микширования (опционально)
                                               nullptr, // Процесс уведомлений (опционально)
                                               &m_sound
                                               );
    
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to load sound: " << filePath << std::endl;
        return;
    }
    
    // Временно отключаем пространственность
    ma_sound_set_spatialization_enabled(&m_sound, false);
    
    m_isInitialized = true;
    
    m_length = GetLength();
}

void AudioComponent::play()
{
    if (!m_isInitialized) return;
    ma_sound_start(&m_sound);
}

void AudioComponent::stop()
{
    if (!m_isInitialized) return;
    ma_sound_stop(&m_sound);
}

void AudioComponent::setLooping(bool loop)
{
    if (!m_isInitialized) return;
    ma_sound_set_looping(&m_sound, loop ? MA_TRUE : MA_FALSE);
}

void AudioComponent::setVolume(float volume)
{
    // volume от 0.0f до 1.0f+
    if (!m_isInitialized) return;
    ma_sound_set_volume(&m_sound, volume);
}

void AudioComponent::destroy()
{
    if (m_isInitialized) {
        ma_sound_uninit(&m_sound);
        m_isInitialized = false;
    }
}

float AudioComponent::GetLength() const
{
    if (!m_isInitialized) return 0.0f;
    ma_uint64 length = 0;
    ma_sound_get_length_in_pcm_frames(const_cast<ma_sound*>(&m_sound), &length);
    
    ma_uint32 sampleRate = ma_engine_get_sample_rate(AudioSystem::Get().GetEngine());
    return (float)length / (float)sampleRate;
}

void AudioComponent::SetCurrentTime(float seconds)
{
    if (!m_isInitialized) return;
    ma_uint32 sampleRate = ma_engine_get_sample_rate(AudioSystem::Get().GetEngine());
    ma_uint64 frameIndex = (ma_uint64)(seconds * sampleRate);
    ma_sound_seek_to_pcm_frame(&m_sound, frameIndex);
}

bool AudioComponent::IsPlaying() const
{
    if (!m_isInitialized) return false;
    return ma_sound_is_playing(&m_sound) == MA_TRUE;
}
