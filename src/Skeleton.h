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
};

struct Skeleton
{
    std::vector<BoneInfo> bones;
};
