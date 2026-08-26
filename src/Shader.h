//
//  Shader.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 26.08.26.
//

#pragma once

#include "RenderDevice.h"

#include <memory>
#include <vector>
#include <unordered_map>

#include <glm/glm.hpp>
#include <glad/glad.h>

class ShaderVariant;

class Shader
{
public:
    void init(const char* filename);
    ShaderVariant* getVariant(FeatureMask featureMask);
    
private:
    std::string m_vertexTemplate;
    std::string m_fragmentTemplate;
    std::unordered_map<uint32_t, std::unique_ptr<ShaderVariant>> m_variants;
    
    
};
