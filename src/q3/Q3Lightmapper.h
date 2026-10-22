#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <map>
#include <glm/glm.hpp>

#include "Q3BSPAsset.h"
#include "TextureAtlas.h"

namespace Q3Lightmapper
{
    struct LightSource
    {
        glm::vec3 position = glm::vec3(0);
        glm::vec3 color = glm::vec3(1);
        float intensity;
        float maxDist;
        float fade;
        
        bool isSun = false;
        glm::vec3 dir;
    };
    
    // Параметры перепаковки:
    // texelsPerUnit — плотность (например, 0.0625f означает 1 тексель на 16 игровых единиц Quake)
    // padding — отступ между островами в пикселях против Texel Bleeding (рекомендуется 4)
    // targetAtlasSize — разрешение финального атласа (например, 2048 или 4096)
    void BakeCustomLightmaps(Q3BSPAsset* bsp,
                             const std::vector<LightSource>& lights,
                             TextureAtlas& outAtlas,
                             float texelsPerUnit = 0.0625f,
                             int padding = 8,
                             int targetAtlasSize = 2048);
}
