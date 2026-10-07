//
//  Grid.cpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 20.07.24.
//

#include "Grid.h"
#include "RenderDevice.h"
#include "VertexBuffer.h"
#include "VertexArray.h"
#include "IndexBuffer.h"

void Grid::init()
{
    float positions[] = {
       -512.0f, 0.0f, -512.0f,
       -512.0f, 0.0f,  512.0f,
        512.0f, 0.0f,  512.0f,
        512.0f, 0.0f, -512.0f,
    };
    
    uint32_t indices[] = {
        0, 1, 2,
        2, 3, 0
    };
    
    vertexArray = new VertexArray();
    
    posBuffer = new VertexBuffer(positions, sizeof(positions));
    posBuffer->setLayout({ { VertexAttributeLocation::Position, ShaderDataType::Float3 } });
    vertexArray->setVertexBuffer(posBuffer);
    
    indexBuffer = new IndexBuffer(indices, 6);
    vertexArray->setIndexBuffer(indexBuffer);
}

void Grid::draw() const
{
    RenderCommand cmd = {
        .vao = vertexArray->getID(),
        .start = 0,
        .count = indexBuffer->getCount(),
        
        .pipeline = PipelineType::Grid,
        .blendMode = BlendMode::Transparent,
    };
    
//    RenderDevice::submit(cmd);
}
