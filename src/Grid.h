//
//  Grid.hpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 20.07.24.
//

#pragma once

#include "Shader.h"

class Camera;

class Grid
{
    unsigned int vbo;
    unsigned int ibo;
    unsigned int vao;
    
public:
//    Grid();
//    ~Grid();
    
    void init();
    void draw() const;
};
