//
//  Grid.cpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 20.07.24.
//

#include <glad/glad.h>
#include "Grid.h"

void Grid::init()
{
    float vertices[] = {
       -512.0f, 0.0f, -512.0f,
       -512.0f, 0.0f,  512.0f,
        512.0f, 0.0f,  512.0f,
        512.0f, 0.0f,  512.0f,
        512.0f, 0.0f, -512.0f,
       -512.0f, 0.0f, -512.0f
    };
    
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 3, vertices, GL_STATIC_DRAW);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    shader.init("assets/shaders/grid.glsl");
}

void Grid::draw(const glm::mat4x4 &viewProjection) const
{
    shader.bind();
    shader.setUniform("MVP", viewProjection);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}
