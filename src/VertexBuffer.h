//
//  VertexBuffer.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 26.07.26.
//

#pragma once

#include "VertexLayout.h"

class VertexBuffer
{
public:
    // Конструктор для статических данных (сразу загружаем в GPU)
    VertexBuffer(const void* data, uint32_t size);
    
    // Конструктор для динамических данных (резервируем место)
    VertexBuffer(uint32_t size);
    
    ~VertexBuffer();
    
    void bind() const;
    void unbind() const;
    
    // Метод для обновления динамических данных
    void setData(const void* data, uint32_t size, uint32_t offset = 0);
    
    void setLayout(const VertexLayout& layout) { m_layout = layout; }
    const VertexLayout& getLayout() const { return m_layout; }
    
    const uint32_t getID() const { return m_id; }
    
private:
    uint32_t m_id = 0;
    VertexLayout m_layout;
};

