//
//  VertexBuffer.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 26.07.26.
//

#pragma once

#include "GpuBuffer.h"
#include "VertexLayout.h"

class VertexBuffer : public GpuBuffer
{
public:
    // Статические данные (сразу загружаем в GPU)
    VertexBuffer(const void* data, uint32_t size);

    // Динамические данные (резервируем место)
    explicit VertexBuffer(uint32_t size);

    // Пустой буфер без хранилища: наполняется через reserve / write / writeTail
    explicit VertexBuffer(BufferUsage usage = BufferUsage::Dynamic);

    ~VertexBuffer() = default;

    void bind() const;
    void unbind() const;

    // Обновление части буфера (то же, что write, оставлено для существующих вызовов)
    void setData(const void* data, uint32_t size, uint32_t offset = 0);

    void setLayout(const VertexLayout& layout) { m_layout = layout; }
    const VertexLayout& getLayout() const { return m_layout; }

private:
    VertexLayout m_layout;
};
