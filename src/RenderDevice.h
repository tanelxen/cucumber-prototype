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
    
    handle_t getWhiteTexture();
    
//    handle_t makeVertexArray(handle_t vertexBuffer, handle_t indexBuffer, const VertexLayout& layout);
//    handle_t makeVertexArray(handle_t posBuffer, const int posLayout, handle_t nrmBuffer, const int nrmLayout, handle_t indexBuffer);
    
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
