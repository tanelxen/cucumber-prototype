//
//  Grid.cpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 20.07.24.
//

#include "Grid.h"
#include "ShaderVariant.h"
#include "RenderDevice.h"
#include "VertexBuffer.h"
#include "VertexArray.h"
#include "IndexBuffer.h"

static ShaderVariant s_grid_shader;

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
    
    s_grid_shader.init("resources/shaders/grid.glsl");
}

void Grid::draw(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) const
{
    s_grid_shader.bind();
    s_grid_shader.setUniform("uVP", projectionMatrix * viewMatrix);
    
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glDepthMask(GL_FALSE);
    
    glBindVertexArray(vertexArray->getID());
    glDrawElements(GL_TRIANGLES, indexBuffer->getCount(), GL_UNSIGNED_INT, (void *)0);
    
    s_grid_shader.unbind();
    
    glDepthMask(GL_TRUE);
}
