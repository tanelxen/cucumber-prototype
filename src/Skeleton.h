//
//  Skeleton.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 21.08.26.
//

#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

struct BoneInfo
{
    std::string name;
    int parentIndex = -1;
    glm::mat4 inverseBindMatrix{1.0f};
    
    // Исходная локальная поза из glTF ноды
    glm::vec3 defaultTranslation = glm::vec3(0.0f);
    glm::quat defaultRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 defaultScale = glm::vec3(1.0f);
};

struct Skeleton
{
    std::vector<BoneInfo> bones;
};
