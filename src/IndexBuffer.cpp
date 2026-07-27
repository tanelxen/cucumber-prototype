//
//  VertexBuffer.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 26.07.26.
//

#include "IndexBuffer.h"
#include <glad/glad.h>

IndexBuffer::IndexBuffer(const uint32_t* indices, uint32_t count) : m_count(count), m_componentType(GL_UNSIGNED_INT)
{
    glGenBuffers(1, &m_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint32_t), indices, GL_STATIC_DRAW);
}

//IndexBuffer::IndexBuffer(const uint16_t* indices, uint32_t count) : m_count(count), m_componentType(GL_UNSIGNED_SHORT)
//{
//    glGenBuffers(1, &m_id);
//    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_id);
//    glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint16_t), indices, GL_STATIC_DRAW);
//}

IndexBuffer::~IndexBuffer()
{
    glDeleteBuffers(1, &m_id);
}

void IndexBuffer::bind() const
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_id);
}

void IndexBuffer::unbind() const
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}
