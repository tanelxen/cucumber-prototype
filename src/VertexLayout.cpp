//
//  VertexBuffer.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 26.07.26.
//

#include "VertexLayout.h"
#include <glad/glad.h>

VertexLayout::VertexLayout(std::initializer_list<VertexAttribute> attributes) : m_attributes(attributes)
{
    calculateOffsetsAndStride();
}

void VertexLayout::calculateOffsetsAndStride()
{
    uint32_t offset = 0;
    m_stride = 0;
    
    for (auto& attribute : m_attributes)
    {
        attribute.offset = offset;
        offset += attribute.size;
        m_stride += attribute.size;
    }
}

static uint32_t ShaderDataTypeSize(ShaderDataType type)
{
    switch (type) {
        case ShaderDataType::Float:  return 4;
        case ShaderDataType::Float2: return 4 * 2;
        case ShaderDataType::Float3: return 4 * 3;
        case ShaderDataType::Float4: return 4 * 4;
        case ShaderDataType::Mat3:   return 4 * 3 * 3;
        case ShaderDataType::Mat4:   return 4 * 4 * 4;
        case ShaderDataType::Int:    return 4;
        case ShaderDataType::Int2:   return 4 * 2;
        case ShaderDataType::Int3:   return 4 * 3;
        case ShaderDataType::Int4:   return 4 * 4;
        case ShaderDataType::Bool:   return 1;
        default: return 0;
    }
}

VertexAttribute::VertexAttribute(VertexAttributeLocation location, ShaderDataType type, bool normalized)
: location(location), type(type), size(ShaderDataTypeSize(type)), offset(0), normalized(normalized)
{
    
}
