//
//  SourceMapScene.h
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 12.05.25.
//

#pragma once

#include <string>
#include <memory>
#include <glm/glm.hpp>

class Entity
{
public:
    Entity(const std::string &name);
    ~Entity();
    
    std::string m_name;
    
    std::unique_ptr<class ShadedCube> m_mesh;
    
    glm::vec3 m_position;
    glm::vec3 m_scale;
    glm::vec4 m_color;
};
