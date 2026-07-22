//
//  RenderDevice.hpp
//  microwolf
//
//  Created by Fedor Artemenkov on 19.03.26.
//

#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>

struct RenderCommand;
class Camera;

struct VertexLayout;

namespace RenderDevice {
    
    typedef unsigned int handle_t;
    
    void init();
    
    handle_t makeTexture(int width, int height, bool hasAlpha, bool srgb, const void* data);
    handle_t makeVertexBuffer(long size, void* data);
    handle_t makeIndexBuffer(long size, void* data);
    
    handle_t makeVertexArray(handle_t vertexBuffer, handle_t indexBuffer, const VertexLayout& layout);
    handle_t makeVertexArray(handle_t posBuffer, const int posLayout, handle_t nrmBuffer, const int nrmLayout, handle_t indexBuffer);
    
    void startFrame();
    void submit(RenderCommand cmd);
    void commit(Camera* camera);
    void endFrame();
}

enum PassType
{
    MainPass            = 1 << 0,
    UserInterfacePass   = 1 << 1,
};

enum class BlendMode
{
    Opaque, Mask, Add, Transparent
};

enum class PipelineType
{
//    World, Skinned
    Generic, FlatShaded, Grid
};

struct RenderCommand
{
    // Mesh
    RenderDevice::handle_t vao;
    RenderDevice::handle_t ibo;
    unsigned int start;
    unsigned int count;
    
    // Material
    PipelineType pipeline;
    RenderDevice::handle_t baseTexture;
    BlendMode blendMode = BlendMode::Opaque;
    glm::vec4 color = {1, 1, 1, 1};
    
    // Object
    glm::mat4 transform = glm::mat4(1);
    
//    uint32_t passFlags = 0;
};

struct VertexAttribute
{
    uint32_t type;
    uint32_t count;
    bool normalized;
    uint32_t offset;
};

struct VertexLayout
{
    std::vector<VertexAttribute> attributes;
    uint32_t stride = 0;
    
    template<typename T>
    void add(uint32_t count) { }
    
    template<>
    void add<float>(uint32_t count)
    {
        VertexAttribute& att = attributes.emplace_back();
        att.type = GL_FLOAT;
        att.count = count;
        att.normalized = false;
        att.offset = stride;
        
        stride += sizeof(GLfloat) * count;
    }
    
    template<>
    void add<unsigned int>(uint32_t count)
    {
        VertexAttribute& att = attributes.emplace_back();
        att.type = GL_UNSIGNED_INT;
        att.count = count;
        att.normalized = false;
        att.offset = stride;
        
        stride += sizeof(GLuint) * count;
    }
    
    template<>
    void add<unsigned char>(uint32_t count)
    {
        VertexAttribute& att = attributes.emplace_back();
        att.type = GL_UNSIGNED_BYTE;
        att.count = count;
        att.normalized = true;
        att.offset = stride;
        
        stride += sizeof(GLbyte) * count;
    }
};
