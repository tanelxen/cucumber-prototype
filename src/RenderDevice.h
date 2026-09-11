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
class Viewport;
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
    
//    void setMainViewport(Viewport* viewport);
    
    void startFrame();
    void submit(RenderCommand cmd);
    void commit(Camera* camera);
    void endFrame();
    
    inline std::vector<glm::vec3> shCoefficients(9, glm::vec3(0.0f));
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
    Generic, Grid, FlatShaded
};

using FeatureMask = uint32_t;

namespace ShaderFeature
{
    constexpr FeatureMask None            = 0;
    constexpr FeatureMask HasAlbedoMap    = 1 << 0;
    constexpr FeatureMask HasNormalMap    = 1 << 1;
    constexpr FeatureMask HasSkinning     = 1 << 2;
    constexpr FeatureMask HasShadows      = 1 << 3;
    constexpr FeatureMask HasSpherical    = 1 << 4;
}

struct RenderCommand
{
    // Mesh
    RenderDevice::handle_t vao;
    RenderDevice::handle_t ibo;
    unsigned int start;
    unsigned int count;
    
    // Material
    PipelineType pipeline;
    FeatureMask featureMask;
    RenderDevice::handle_t baseTexture;
    BlendMode blendMode = BlendMode::Opaque;
    glm::vec4 color = {1, 1, 1, 1};
    
    glm::mat4 transform = glm::mat4(1);
    
    const std::vector<glm::mat4>* bones;
};
