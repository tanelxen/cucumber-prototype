//
//  Shader.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 26.08.26.
//

#include "Shader.h"
#include "ShaderVariant.h"

#include <iostream>
#include <fstream>
#include <sstream>

void Shader::init(const char *filepath)
{
    enum ShaderSourceType {
        SRC_NONE = -1,
        SRC_VERTEX = 0,
        SCR_FRAGMENT = 1
    };
    
    std::ifstream stream(filepath);
    
    if (!stream.is_open())
    {
        std::cerr << "Error: Failed to open the file at path: " << filepath << std::endl;
        return;
    }
    
    std::string line;
    std::stringstream vertexStream;
    std::stringstream fragmentStream;
    
    ShaderSourceType currentType = SRC_NONE;
    
    while (getline(stream, line))
    {
        if (line.find("#shader") != std::string::npos)
        {
            if (line.find("vertex") != std::string::npos)
            {
                currentType = SRC_VERTEX;
                
            }
            else if (line.find("fragment") != std::string::npos)
            {
                currentType = SCR_FRAGMENT;
            }
        }
        else if (currentType == SRC_VERTEX)
        {
            vertexStream << line << "\n";
        }
        else if (currentType == SCR_FRAGMENT)
        {
            fragmentStream << line << "\n";
        }
    }
    
    m_vertexTemplate = vertexStream.str();
    m_fragmentTemplate = fragmentStream.str();
}

ShaderVariant* Shader::getVariant(FeatureMask key)
{
    auto it = m_variants.find(key);
    if (it != m_variants.end()) {
        return it->second.get();
    }
    
    std::string defines = "#version 410 core\n";
    
    if (key & ShaderFeature::HasAlbedoMap) defines += "#define HAS_ALBEDO_MAP\n";
    if (key & ShaderFeature::HasNormalMap) defines += "#define HAS_NORMAL_MAP\n";
    if (key & ShaderFeature::HasSkinning)  defines += "#define HAS_SKINNING\n";
    if (key & ShaderFeature::HasShadows)   defines += "#define HAS_SHADOWS\n";
    if (key & ShaderFeature::HasSpherical)   defines += "#define HAS_SH_LIGHT\n";
    if (key & ShaderFeature::HasLightMap)   defines += "#define HAS_LIGHT_MAP\n";
    
    std::string vs_source = defines + m_vertexTemplate;
    std::string fs_source = defines + m_fragmentTemplate;
    
    auto newVariant = std::make_unique<ShaderVariant>();
    newVariant->init(vs_source.c_str(), fs_source.c_str());
    ShaderVariant* ptr = newVariant.get();
    
    m_variants[key] = std::move(newVariant);
    return ptr;
}
