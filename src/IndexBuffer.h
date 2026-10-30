//
//  IndexBuffer.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 26.07.26.
//

#pragma once

#include <cstdint>
#include <glad/glad.h>

class IndexBuffer
{
public:
    IndexBuffer(const uint32_t* indices, uint32_t count);
    
    ~IndexBuffer();
    
    void bind() const;
    void unbind() const;
    
    uint32_t getCount() const { return m_count; }
    
    const uint32_t getID() const { return m_id; }
    
private:
    uint32_t m_id = 0;
    uint32_t m_count = 0;
    uint32_t m_componentType;
};

