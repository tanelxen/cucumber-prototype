//
//  Grid.hpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 20.07.24.
//

#pragma once

class VertexBuffer;
class VertexArray;
class IndexBuffer;

#include <glm/glm.hpp>

class Grid
{
public:
    void init();
    void draw(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) const;
    
private:
    VertexBuffer* posBuffer;
    IndexBuffer* indexBuffer;
    VertexArray* vertexArray;
};
