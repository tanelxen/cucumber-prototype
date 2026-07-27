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

class Grid
{
public:
    void init();
    void draw() const;
    
private:
    VertexBuffer* posBuffer;
    IndexBuffer* indexBuffer;
    VertexArray* vertexArray;
};
