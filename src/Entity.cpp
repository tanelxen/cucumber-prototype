//
//  SourceMapScene.cpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 12.05.25.
//

#include "Entity.h"
#include "Cube.h"

Entity::Entity(const std::string &name) : m_name(name)
{
    m_mesh = std::make_unique<ShadedCube>();
    m_mesh->init();
}

Entity::~Entity()
{
    
}
