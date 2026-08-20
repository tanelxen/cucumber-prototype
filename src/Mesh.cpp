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

void Mesh::draw(glm::mat4& transform) const
{
    for (const auto& surface : m_surfaces)
    {
        auto& material = m_materials[surface.materialSlot];
        
        RenderCommand cmd = {
            .vao = vertexArray->getID(),
            .start = surface.startIndex,
            .count = surface.count,
            
            .pipeline = PipelineType::Generic,
            .blendMode = material.blendMode,
            .baseTexture = material.albedoMap,
            .color = material.tintColor,
            
            .transform = transform,
        };
        
        RenderDevice::submit(cmd);
    }
}

void Mesh::setPositions(const std::vector<glm::vec3> &positions)
{
    if (vertexArray == nullptr) {
        vertexArray = new VertexArray();
    }
    
    posBuffer = new VertexBuffer(positions.data(), sizeof(glm::vec3) * (uint32_t)positions.size());
    posBuffer->setLayout({ { VertexAttributeLocation::Position, ShaderDataType::Float3 } });
    vertexArray->setVertexBuffer(posBuffer);
}

void Mesh::setNormals(const std::vector<glm::vec3> &normals)
{
    if (vertexArray == nullptr) {
        vertexArray = new VertexArray();
    }
    
    nrmBuffer = new VertexBuffer(normals.data(), sizeof(glm::vec3) * (uint32_t)normals.size());
    nrmBuffer->setLayout({ { VertexAttributeLocation::Normal, ShaderDataType::Float3 } });
    vertexArray->setVertexBuffer(nrmBuffer);
}

void Mesh::setTexCoords(const std::vector<glm::vec2> &uv)
{
    if (vertexArray == nullptr) {
        vertexArray = new VertexArray();
    }
    
    texBuffer = new VertexBuffer(uv.data(), sizeof(glm::vec2) * (uint32_t)uv.size());
    texBuffer->setLayout({ { VertexAttributeLocation::TexCoord, ShaderDataType::Float2 } });
    vertexArray->setVertexBuffer(texBuffer);
}

void Mesh::setIndices(const std::vector<uint32_t> &indices)
{
    if (vertexArray == nullptr) {
        vertexArray = new VertexArray();
    }
    
    indexBuffer = new IndexBuffer(indices.data(), (uint32_t)indices.size());
    vertexArray->setIndexBuffer(indexBuffer);
}

void Mesh::setSkinned(const std::vector<glm::ivec4> &boneIndices, const std::vector<glm::vec4> &weights)
{
    bonesBuffer = new VertexBuffer(boneIndices.data(), sizeof(glm::ivec4) * (uint32_t)boneIndices.size());
    bonesBuffer->setLayout({ { VertexAttributeLocation::BoneIndices, ShaderDataType::Int4 } });
    vertexArray->setVertexBuffer(bonesBuffer);
    
    weightsBuffer = new VertexBuffer(weights.data(), sizeof(glm::vec4) * (uint32_t)weights.size());
    weightsBuffer->setLayout({ { VertexAttributeLocation::BoneWeights, ShaderDataType::Float4 } });
    vertexArray->setVertexBuffer(weightsBuffer);
}

void Mesh::update(float dt)
{
}


