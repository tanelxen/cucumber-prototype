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

#include "glm/gtc/matrix_transform.hpp"

#include "ModelLoader.h"

//#define STB_IMAGE_WRITE_IMPLEMENTATION
//#include "../vendor/stb_image_write.h"

//#define TINYOBJLOADER_IMPLEMENTATION
//#include "../vendor/tiny_obj_loader.h"


void Mesh::initCube()
{
    std::vector<glm::vec3> positions = {
        // Передняя грань
        {0.0, 0.0, 1.0},
        {1.0, 0.0, 1.0},
        {1.0, 1.0, 1.0},
        {0.0, 1.0, 1.0},
        // Задняя грань
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {1.0, 1.0, 0.0},
        {0.0, 1.0, 0.0},
        // Правая грань
        {1.0, 0.0, 0.0},
        {1.0, 1.0, 0.0},
        {1.0, 1.0, 1.0},
        {1.0, 0.0, 1.0},
        // Левая грань
        {0.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 1.0, 1.0},
        {0.0, 0.0, 1.0},
        // Верхняя грань
        {0.0, 1.0, 1.0},
        {1.0, 1.0, 1.0},
        {1.0, 1.0, 0.0},
        {0.0, 1.0, 0.0},
        // Нижняя грань
        {0.0, 0.0, 1.0},
        {1.0, 0.0, 1.0},
        {1.0, 0.0, 0.0},
        {0.0, 0.0, 0.0},
    };
    
    std::vector<glm::vec3> normals = {
        // Передняя грань
        {0.0f,  0.0f,  1.0f},
        {0.0f,  0.0f,  1.0f},
        {0.0f,  0.0f,  1.0f},
        {0.0f,  0.0f,  1.0f},
        // Задняя грань
        {0.0f,  0.0f, -1.0f},
        {0.0f,  0.0f, -1.0f},
        {0.0f,  0.0f, -1.0f},
        {0.0f,  0.0f, -1.0f},
        // Левая грань
        {-1.0f,  0.0f,  0.0f},
        {-1.0f,  0.0f,  0.0f},
        {-1.0f,  0.0f,  0.0f},
        {-1.0f,  0.0f,  0.0f},
        // Правая грань
        {1.0f,  0.0f,  0.0f},
        {1.0f,  0.0f,  0.0f},
        {1.0f,  0.0f,  0.0f},
        {1.0f,  0.0f,  0.0f},
        // Верхняя грань
        {0.0f,  1.0f,  0.0f},
        {0.0f,  1.0f,  0.0f},
        {0.0f,  1.0f,  0.0f},
        {0.0f,  1.0f,  0.0f},
        // Нижняя грань
        {0.0f, -1.0f,  0.0f},
        {0.0f, -1.0f,  0.0f},
        {0.0f, -1.0f,  0.0f},
        {0.0f, -1.0f,  0.0f},
    };
    
    std::vector<uint32_t> indices = {
        0, 1, 2, 2, 3, 0,    // Передняя грань
        4, 7, 6, 6, 5, 4,    // Задняя грань
        8, 9, 10, 10, 11, 8, // Левая грань
        12, 15, 14, 14, 13, 12, // Правая грань
        16, 17, 18, 18, 19, 16, // Верхняя грань
        20, 23, 22, 22, 21, 20  // Нижняя грань
    };
    
    init(positions, normals, indices);
    m_name = "Cube";
}

void Mesh::init(const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& normals, const std::vector<uint32_t>& indices)
{
    vertexArray = new VertexArray();
    
    posBuffer = new VertexBuffer(positions.data(), sizeof(glm::vec3) * (uint32_t)positions.size());
    posBuffer->setLayout({ { VertexAttributeLocation::Position, ShaderDataType::Float3 } });
    vertexArray->setVertexBuffer(posBuffer);
    
    nrmBuffer = new VertexBuffer(normals.data(), sizeof(glm::vec3) * (uint32_t)normals.size());
    nrmBuffer->setLayout({ { VertexAttributeLocation::Normal, ShaderDataType::Float3 } });
    vertexArray->setVertexBuffer(nrmBuffer);
    
    indexBuffer = new IndexBuffer(indices.data(), (uint32_t)indices.size());
    vertexArray->setIndexBuffer(indexBuffer);
}

void Mesh::init(const std::vector<float>& positions, const std::vector<float>& normals, const std::vector<uint32_t>& indices)
{
    vertexArray = new VertexArray();
    
    posBuffer = new VertexBuffer(positions.data(), sizeof(float) * (uint32_t)positions.size());
    posBuffer->setLayout({ { VertexAttributeLocation::Position, ShaderDataType::Float3 } });
    vertexArray->setVertexBuffer(posBuffer);
    
    nrmBuffer = new VertexBuffer(normals.data(), sizeof(float) * (uint32_t)normals.size());
    nrmBuffer->setLayout({ { VertexAttributeLocation::Normal, ShaderDataType::Float3 } });
    vertexArray->setVertexBuffer(nrmBuffer);
    
    indexBuffer = new IndexBuffer(indices.data(), (uint32_t)indices.size());
    vertexArray->setIndexBuffer(indexBuffer);
}

void Mesh::draw(glm::mat4& transform, glm::vec4 tintColor) const
{
    for (const auto& surface : m_surfaces)
    {
//        auto& material = m_textures[surface.textureNum];
        
        RenderCommand cmd = {
            .vao = vertexArray->getID(),
            .start = surface.IndexStart,
            .count = surface.IndexCount,
            
            .pipeline = PipelineType::FlatShaded,
            .blendMode = BlendMode::Opaque,
            .color = tintColor,
            
            .transform = transform,
        };
        
        RenderDevice::submit(cmd);
    }
}
