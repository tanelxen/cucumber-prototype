//
//  VertexBuffer.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 26.07.26.
//

#include "VertexBuffer.h"
#include <glad/glad.h>

VertexBuffer::VertexBuffer(const void* data, uint32_t size) : GpuBuffer(BufferUsage::Static)
{
    allocate(data, size);
}

VertexBuffer::VertexBuffer(uint32_t size) : GpuBuffer(BufferUsage::Dynamic)
{
    allocate(nullptr, size);
}

VertexBuffer::VertexBuffer(BufferUsage usage) : GpuBuffer(usage)
{
}

void VertexBuffer::bind() const
{
    glBindBuffer(GL_ARRAY_BUFFER, getID());
}

void VertexBuffer::unbind() const
{
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void VertexBuffer::setData(const void* data, uint32_t size, uint32_t offset)
{
    write(offset, data, size);
}
