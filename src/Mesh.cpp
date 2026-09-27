//
//  Cube.cpp
//  TryOpenGL
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

void Mesh::draw(glm::mat4& transform) const
{
    for (const auto& surface : m_surfaces)
    {
        auto& material = m_materials[surface.materialSlot];
        
        uint32_t features = 0;
        
        if (material.albedoMap > 0)
        {
            features |= ShaderFeature::HasAlbedoMap;
        }
        
//        if (m_useAmbientLight)
//        {
//            features |= ShaderFeature::HasSpherical;
//        }
        
        RenderCommand cmd = {
            .vao = m_vao->getID(),
            .start = surface.startIndex,
            .count = surface.count,
            
            .pipeline = PipelineType::Generic,
            .featureMask = features,
            .blendMode = material.blendMode,
            .baseTexture = material.albedoMap,
            .color = material.tintColor,
            
            .transform = transform
        };
        
        RenderDevice::submit(cmd);
    }
}

void Mesh::draw(glm::mat4& transform, const std::vector<glm::mat4>& bonesPalette) const
{
    for (const auto& surface : m_surfaces)
    {
        auto& material = m_materials[surface.materialSlot];
        
        uint32_t features = ShaderFeature::HasSkinning;
        
        if (material.albedoMap > 0)
        {
            features |= ShaderFeature::HasAlbedoMap;
        }
        
//        if (m_useAmbientLight)
//        {
//            features |= ShaderFeature::HasSpherical;
//        }
        
        RenderCommand cmd = {
            .vao = m_vao->getID(),
            .start = surface.startIndex,
            .count = surface.count,
            
            .pipeline = PipelineType::Generic,
            .featureMask = features,
            .blendMode = material.blendMode,
            .baseTexture = material.albedoMap,
            .color = material.tintColor,
            
            .transform = transform,
            .bones = &bonesPalette
        };
        
        RenderDevice::submit(cmd);
    }
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
}

