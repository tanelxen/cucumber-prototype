//
//  RenderDevice.h
//  Cucumber
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
    
    void startFrame();
    void submit(RenderCommand cmd);
    void commit(Camera* camera);
    void endFrame();
    
    inline std::vector<glm::vec3> shCoefficients(9, glm::vec3(0.0f));
}

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
    constexpr FeatureMask HasLightMap     = 1 << 5;
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
    RenderDevice::handle_t lightMap;
    BlendMode blendMode = BlendMode::Opaque;
    bool reverseWinding = false;
    glm::vec4 color = {1, 1, 1, 1};
    
    glm::mat4 transform = glm::mat4(1);
    
    // Animation
    const std::vector<glm::mat4>* bones;
};
