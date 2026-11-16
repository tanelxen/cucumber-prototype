//
//  IndexBuffer.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 26.07.26.
//

#include "IndexBuffer.h"

IndexBuffer::IndexBuffer(const uint32_t* indices, uint32_t count) : GpuBuffer(BufferUsage::Static), m_count(count)
{
    // Раньше конструктор делал bind(GL_ELEMENT_ARRAY_BUFFER): если в этот момент был привязан чужой VAO,
    // ему молча подменялись индексы. allocate() идёт через GL_COPY_WRITE_BUFFER и этого не делает.
    allocate(indices, static_cast<size_t>(count) * sizeof(uint32_t));
}

IndexBuffer::IndexBuffer(BufferUsage usage) : GpuBuffer(usage)
{
}

void IndexBuffer::bind() const
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, getID());
}

void IndexBuffer::unbind() const
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}
