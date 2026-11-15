//
//  GpuBuffer.cpp
//  Cucumber
//

#include "GpuBuffer.h"

#include <glad/glad.h>

#include <algorithm>

namespace
{
    constexpr GLenum kScratchTarget = GL_COPY_WRITE_BUFFER;

    GLenum ToGLUsage(BufferUsage usage)
    {
        return usage == BufferUsage::Static ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW;
    }
}

GpuBuffer::GpuBuffer(BufferUsage usage) : m_usage(usage)
{
    glGenBuffers(1, &m_id);
}

GpuBuffer::~GpuBuffer()
{
    glDeleteBuffers(1, &m_id);
}

void GpuBuffer::allocate(const void* data, size_t bytes)
{
    glBindBuffer(kScratchTarget, m_id);
    glBufferData(kScratchTarget, static_cast<GLsizeiptr>(bytes), data, ToGLUsage(m_usage));
    glBindBuffer(kScratchTarget, 0);
    m_capacity = bytes;
}

bool GpuBuffer::reserve(size_t bytes)
{
    if (bytes <= m_capacity) return false;

    allocate(nullptr, std::max(bytes, m_capacity + m_capacity / 2));
    return true;
}

void GpuBuffer::write(size_t offsetBytes, const void* data, size_t bytes)
{
    if (bytes == 0) return;

    glBindBuffer(kScratchTarget, m_id);
    glBufferSubData(kScratchTarget, static_cast<GLintptr>(offsetBytes), static_cast<GLsizeiptr>(bytes), data);
    glBindBuffer(kScratchTarget, 0);
}

size_t GpuBuffer::writeTail(const void* data, size_t count, size_t elementSize, size_t first)
{
    if (reserve(count * elementSize)) first = 0;   // хранилище новое: префикса в нём нет

    first = std::min(first, count);
    const size_t tailBytes = (count - first) * elementSize;
    write(first * elementSize, static_cast<const char*>(data) + first * elementSize, tailBytes);
    return tailBytes;
}
