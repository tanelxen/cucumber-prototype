//
//  Mesh.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 21.10.24.
//

#include "Mesh.h"

#include "RenderDevice.h"
#include "VertexBuffer.h"
#include "VertexArray.h"
#include "IndexBuffer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

Mesh::Mesh() = default;
Mesh::~Mesh() = default;

unsigned int Mesh::getVAOId() const
{
    return m_vao ? m_vao->getID() : 0;
}

void Mesh::setIndices(const std::vector<uint32_t> &indices)
{
    m_indexBuffer = std::make_unique<IndexBuffer>(indices.data(), indices.size());
    m_vao->setIndexBuffer(m_indexBuffer.get());
}

void Mesh::setVertexData(const std::vector<VertexPosition>& positions,
                         const std::vector<VertexShading>& shadingData,
                         const std::vector<VertexSkinning>* skinningData)
{
    m_vao = std::make_unique<VertexArray>();
    
    // Stream 0: Position
    m_posBuffer = std::make_unique<VertexBuffer>(positions.data(), positions.size() * sizeof(VertexPosition));
    m_posBuffer->setLayout({
        { VertexAttributeLocation::Position, ShaderDataType::Float3 }
    });
    m_vao->setVertexBuffer(m_posBuffer.get());
    
    // Stream 1: Shading
    m_shadingBuffer = std::make_unique<VertexBuffer>(shadingData.data(), shadingData.size() * sizeof(VertexShading));
    m_shadingBuffer->setLayout({
        { VertexAttributeLocation::Normal, ShaderDataType::Float3 },
        { VertexAttributeLocation::TexCoord, ShaderDataType::Float2 }
    });
    m_vao->setVertexBuffer(m_shadingBuffer.get());
    
    // Stream 2: Skinning (только если меш имеет скелет)
    if (skinningData && !skinningData->empty())
    {
        m_skinningBuffer = std::make_unique<VertexBuffer>(skinningData->data(), skinningData->size() * sizeof(VertexSkinning));
        m_skinningBuffer->setLayout({
            { VertexAttributeLocation::BoneIndices, ShaderDataType::UByte4 },
            { VertexAttributeLocation::BoneWeights, ShaderDataType::UByte4, true }
        });
        m_vao->setVertexBuffer(m_skinningBuffer.get());
    }
    
    m_bounds = AABB{};
    for (const auto& v : positions)
    {
        m_bounds.expand(v.position);
    }
}

