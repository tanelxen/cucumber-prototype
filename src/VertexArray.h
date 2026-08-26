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
    
//    void AddVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer, const Shader& shader);
    void setVertexBuffer(const VertexBuffer* vertexBuffer);
    void setIndexBuffer(const IndexBuffer* indexBuffer);
    
//    const std::vector<std::shared_ptr<VertexBuffer>>& GetVertexBuffers() const { return m_VertexBuffers; }
//    const std::shared_ptr<IndexBuffer>& GetIndexBuffer() const { return m_IndexBuffer; }
    
    const uint32_t getID() const { return m_id; }
    
private:
    uint32_t m_id = 0;
//    std::vector<std::shared_ptr<VertexBuffer>> m_VertexBuffers;
//    std::shared_ptr<IndexBuffer> m_IndexBuffer;
//    IndexBuffer* m_indexBuffer;
};

