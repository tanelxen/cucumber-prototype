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
    
    void save(nlohmann::json& j) const override;
    void load(const nlohmann::json& j) override;
    
    void init(const std::string& filePath);
    
    void update(float dt) override;
    
    void play();
    void stop();
    
    void setLooping(bool loop);
    void setVolume(float volume);
    
    void destroy();
    
    float GetCurrentTime() const;
    
    // Возвращает общую длину трека в секундах
    float GetLength() const;
    
    // Позволяет перематывать трек (нужно для прогресс-бара, если делать его интерактивным)
    void SetCurrentTime(float seconds);
    
    bool IsPlaying() const;
    
    // Метод для получения текущих настроек (нужен для ImGui)
    float GetVolume() const { return m_isInitialized ? ma_sound_get_volume(&m_sound) : 0.0f; }
    bool IsLooping() const { return m_isInitialized ? (ma_sound_is_looping(&m_sound) == MA_TRUE) : false; }
    
};
