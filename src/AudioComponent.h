//
//  AudioComponent.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 09.09.26.
//

#pragma once

#include "Entity.h"
#include "AudioSystem.h"

struct AudioComponent : public Component
{
    AudioComponent();
    
    std::string path = "<no sound>";
    
    ma_sound m_sound;
    bool m_isInitialized = false;
    float m_length;
    
    const std::string getTypeName() const override { return "AudioComponent"; }
    
    void save(nlohmann::json& j) const override
    {
        j = nlohmann::json{ {"source", path} };
    }
    
    void load(const nlohmann::json& j) override
    {
        path = j.at("source");
    }
    
    void init(const std::string& filePath)
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
    
    void update(float dt) override;
    
    void play()
    {
        if (!m_isInitialized) return;
        ma_sound_start(&m_sound);
    }
    
    void stop() {
        if (!m_isInitialized) return;
        ma_sound_stop(&m_sound);
    }
    
    void setLooping(bool loop) {
        if (!m_isInitialized) return;
        ma_sound_set_looping(&m_sound, loop ? MA_TRUE : MA_FALSE);
    }
    
    void setVolume(float volume) { // volume от 0.0f до 1.0f+
        if (!m_isInitialized) return;
        ma_sound_set_volume(&m_sound, volume);
    }
    
    void destroy()
    {
        if (m_isInitialized) {
            ma_sound_uninit(&m_sound);
            m_isInitialized = false;
        }
    }
    
    float GetCurrentTime() const;
    
    // Возвращает общую длину трека в секундах
    float GetLength() const {
        if (!m_isInitialized) return 0.0f;
        ma_uint64 length = 0;
        ma_sound_get_length_in_pcm_frames(const_cast<ma_sound*>(&m_sound), &length);
        
        ma_uint32 sampleRate = ma_engine_get_sample_rate(AudioSystem::Get().GetEngine());
        return (float)length / (float)sampleRate;
    }
    
    // Позволяет перематывать трек (нужно для прогресс-бара, если делать его интерактивным)
    void SetCurrentTime(float seconds) {
        if (!m_isInitialized) return;
        ma_uint32 sampleRate = ma_engine_get_sample_rate(AudioSystem::Get().GetEngine());
        ma_uint64 frameIndex = (ma_uint64)(seconds * sampleRate);
        ma_sound_seek_to_pcm_frame(&m_sound, frameIndex);
    }
    
    bool IsPlaying() const {
        if (!m_isInitialized) return false;
        return ma_sound_is_playing(&m_sound) == MA_TRUE;
    }
    
    // Метод для получения текущих настроек (нужен для ImGui)
    float GetVolume() const { return m_isInitialized ? ma_sound_get_volume(&m_sound) : 0.0f; }
    bool IsLooping() const { return m_isInitialized ? (ma_sound_is_looping(&m_sound) == MA_TRUE) : false; }
    
};
