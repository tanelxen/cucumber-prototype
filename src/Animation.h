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
struct AnimationTrack {
    int boneIndex;
    
    std::vector<float> positionTimes;
    std::vector<glm::vec3> positions;
    
    std::vector<float> rotationTimes;
    std::vector<glm::quat> rotations;
    
    std::vector<float> scaleTimes;
    std::vector<glm::vec3> scales;
};

struct AnimationClip {
    std::string name;
    float duration = 0.0f;
    std::vector<AnimationTrack> tracks;
};
