//
//  Sky.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 01.09.26.
//

#pragma once

#include <glm/glm.hpp>

class Sky
{
public:
    void init();
    void draw(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) const;
};
