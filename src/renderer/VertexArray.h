//
//  VertexBuffer.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 26.07.26.
//

#pragma once

#include "VertexLayout.h"

#include <memory>
#include <vector>

class VertexBuffer;
class IndexBuffer;
class ShaderVariant;

class VertexArray
{
public:
    VertexArray();
    ~VertexArray();
    
    void bind() const;
    void unbind() const;
    
    void setVertexBuffer(const VertexBuffer* vertexBuffer);
    void setIndexBuffer(const IndexBuffer* indexBuffer);
    
    const uint32_t getID() const { return m_id; }
    
private:
    uint32_t m_id = 0;
};

