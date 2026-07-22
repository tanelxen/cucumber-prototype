//
//  Grid.cpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 20.07.24.
//

#include "Grid.h"
#include "RenderDevice.h"

void Grid::init()
{
    float vertices[] = {
       -512.0f, 0.0f, -512.0f,
       -512.0f, 0.0f,  512.0f,
        512.0f, 0.0f,  512.0f,
        512.0f, 0.0f, -512.0f,
    };
    
    int indices[] = {
        0, 1, 2,
        2, 3, 0
    };
    
    vbo = RenderDevice::makeVertexBuffer(sizeof(vertices), (void*)vertices);
    ibo = RenderDevice::makeIndexBuffer(sizeof(indices), (void*)indices);
    
    VertexLayout layout;
    layout.add<float>(3);
    
    vao = RenderDevice::makeVertexArray(vbo, ibo, layout);
}

void Grid::draw() const
{
    RenderCommand cmd = {
        .vao = vao,
        .start = 0,
        .count = 6,
        
        .pipeline = PipelineType::Grid,
        .blendMode = BlendMode::Transparent,
    };
    
    RenderDevice::submit(cmd);
}
