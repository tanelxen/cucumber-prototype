//
//  Animation.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 21.08.26.
//

#pragma once

#include <glm/gtc/quaternion.hpp>
#include <string>

// Хранит ключи для конкретной кости
struct AnimationTrack
{
    int boneIndex;
    
    std::vector<float> positionTimes;
    std::vector<glm::vec3> positions;
    
    std::vector<float> rotationTimes;
    std::vector<glm::quat> rotations;
    
    std::vector<float> scaleTimes;
    std::vector<glm::vec3> scales;
};

struct AnimationEvent
{
    float time;               // Время в секундах от начала анимации, когда должно сработать событие
    std::string name;         // Имя события (например, "sound_mag_out", "fx_muzzle_flash")
    std::string paramString;  // Дополнительный параметр (например, путь к файлу звука)
    bool hasFired = false;    // Флаг-предохранитель, чтобы событие не вызывалось каждый кадр
};

struct AnimationClip
{
    std::string name;
    float duration = 0.0f;
    std::vector<AnimationTrack> tracks;
    
    std::vector<AnimationEvent> events;
};
