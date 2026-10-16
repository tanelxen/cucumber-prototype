//
//  AudioSystem.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 01.09.26.
//

#pragma once

#include "ProjectManager.h"
#include "miniaudio.h"
#include <iostream>

class AudioSystem
{
public:
    static AudioSystem& Get()
    {
        static AudioSystem instance;
        return instance;
    }
    
    bool Initialize()
    {
        ma_result result = ma_engine_init(nullptr, &m_engine);
        if (result != MA_SUCCESS) {
            std::cerr << "Failed to initialize miniaudio engine." << std::endl;
            return false;
        }
        return true;
    }
    
    void Shutdown()
    {
        ma_engine_uninit(&m_engine);
    }
    
    ma_engine* GetEngine() { return &m_engine; }
    
    void PlaySound(const std::string& filename)
    {
        auto path = ProjectManager::Get().assetsPath() / filename;
        ma_engine_play_sound(&m_engine, path.string().c_str(), NULL);
    }
    
private:
    AudioSystem() = default;
    ~AudioSystem() { Shutdown(); }
    
    ma_engine m_engine;
};

