//
//  VertexLayout.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 26.07.26.
//

#pragma once

#include <vector>
#include <cstdint>

enum class ShaderDataType
{
    None = 0, Float, Float2, Float3, Float4, Mat3, Mat4, Int, Int2, Int3, Int4, Bool
};

enum VertexAttributeLocation : uint32_t
{
    Position = 0,
    Normal   = 1,
    TexCoord = 2,
};

struct VertexAttribute
{
    VertexAttributeLocation location;
    ShaderDataType type;
    uint32_t size;
    uint32_t offset;
    bool normalized;
    
    VertexAttribute(VertexAttributeLocation location, ShaderDataType type, bool normalized = false);
};

class VertexLayout
{
public:
    VertexLayout(std::initializer_list<VertexAttribute> attributes);
    VertexLayout() = default;
    
    uint32_t getStride() const { return m_stride; }
    const std::vector<VertexAttribute>& getAttributes() const { return m_attributes; }
    
    // (for (auto& attr : layout))
    std::vector<VertexAttribute>::iterator begin() { return m_attributes.begin(); }
    std::vector<VertexAttribute>::iterator end() { return m_attributes.end(); }
    std::vector<VertexAttribute>::const_iterator begin() const { return m_attributes.begin(); }
    std::vector<VertexAttribute>::const_iterator end() const { return m_attributes.end(); }
    
private:
    void calculateOffsetsAndStride();
    
private:
    std::vector<VertexAttribute> m_attributes;
    uint32_t m_stride = 0;
};

