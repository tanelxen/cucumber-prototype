//
//  IndexBuffer.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 26.07.26.
//

#pragma once

#include "GpuBuffer.h"

#include <cstdint>
#include <glad/glad.h>   // оставлен: другие файлы могут получать glad транзитивно

class IndexBuffer : public GpuBuffer
{
public:
    // Статические индексы (сразу загружаем в GPU)
    IndexBuffer(const uint32_t* indices, uint32_t count);

    // Пустой буфер без хранилища: наполняется через reserve / write / writeTail
    explicit IndexBuffer(BufferUsage usage = BufferUsage::Dynamic);

    ~IndexBuffer() = default;

    // Внимание: bind() под привязанным VAO меняет индексный буфер этого VAO.
    // Для заливки данных используйте write / writeTail, они VAO не трогают.
    void bind() const;
    void unbind() const;

    uint32_t getCount() const { return m_count; }

    // Для динамического буфера: сколько индексов в нём сейчас лежит
    void setCount(uint32_t count) { m_count = count; }

private:
    uint32_t m_count = 0;
};
