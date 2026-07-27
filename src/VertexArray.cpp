//
//  VertexBuffer.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 26.07.26.
//

#include "VertexArray.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Shader.h"

#include <glad/glad.h>

VertexArray::VertexArray()
{
    glGenVertexArrays(1, &m_id);
}

VertexArray::~VertexArray()
{
    glDeleteVertexArrays(1, &m_id);
}

void VertexArray::bind() const
{
    glBindVertexArray(m_id);
}

void VertexArray::unbind() const
{
    glBindVertexArray(0);
}

static uint32_t GetComponentCount(ShaderDataType type)
{
    switch (type)
    {
        case ShaderDataType::Float:  return 1;
        case ShaderDataType::Float2: return 2;
        case ShaderDataType::Float3: return 3;
        case ShaderDataType::Float4: return 4;
        case ShaderDataType::Int:    return 1;
        case ShaderDataType::Int2:   return 2;
        case ShaderDataType::Int3:   return 3;
        case ShaderDataType::Int4:   return 4;
        case ShaderDataType::Bool:   return 1;
        default: return 0;
    }
}

static GLenum ShaderDataTypeToOpenGLBaseType(ShaderDataType type)
{
    switch (type)
    {
        case ShaderDataType::Float:
        case ShaderDataType::Float2:
        case ShaderDataType::Float3:
        case ShaderDataType::Float4:
        case ShaderDataType::Mat3:
        case ShaderDataType::Mat4: return GL_FLOAT;
        case ShaderDataType::Int:
        case ShaderDataType::Int2:
        case ShaderDataType::Int3:
        case ShaderDataType::Int4: return GL_INT;
        case ShaderDataType::Bool: return GL_BOOL;
        default: return 0;
    }
}

void VertexArray::setVertexBuffer(const VertexBuffer* vertexBuffer)
{
    bind();
    vertexBuffer->bind();
    
    const auto& layout = vertexBuffer->getLayout();
    
    for (const auto& element : layout)
    {
        uint32_t location = static_cast<uint32_t>(element.location);
        
        glEnableVertexAttribArray(location);
        
        glVertexAttribPointer(
                              location,
                              GetComponentCount(element.type),
                              ShaderDataTypeToOpenGLBaseType(element.type),
                              element.normalized ? GL_TRUE : GL_FALSE,
                              layout.getStride(),
                              (const void*)(uintptr_t)element.offset
                              );
    }
    
    vertexBuffer->unbind();
    unbind();
    
//    m_VertexBuffers.push_back(vertexBuffer);
}

void VertexArray::setIndexBuffer(const IndexBuffer* indexBuffer)
{
    bind();
    indexBuffer->bind();
//    m_IndexBuffer = indexBuffer;
    
    unbind();
    indexBuffer->unbind();
}
