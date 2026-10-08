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
class RenderView;

struct VertexLayout;

namespace RenderDevice {
    
    typedef unsigned int handle_t;
    
    void init();
    
    handle_t makeTexture(int width, int height, bool hasAlpha, bool srgb, const void* data);
    handle_t makeVertexBuffer(long size, void* data);
    handle_t makeIndexBuffer(long size, void* data);
    
    handle_t getWhiteTexture();
    
    void startFrame();
//    void submit(RenderCommand cmd);
//    void commit(Camera* camera);
//    void commit(const glm::mat4& view, const glm::mat4& projection);
    void endFrame();
    
//    void executePasses(const std::vector<RenderView>& passes, bool drawSkybox, bool isSimulationMode);
    
    inline std::vector<glm::vec3> shCoefficients(9, glm::vec3(0.0f));
    
    
    // Регистрация прохода рендера для камеры
    void submitView(const RenderView& view);
    
    // Финальный вызов, который отсортирует проходы, вызовет draw и очистит списки
    void flush();
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

namespace Layers
{
    const uint32_t Default    = 1 << 0; // 1  (Обычные объекты мира)
    const uint32_t Static     = 1 << 1; // 2  (BSP геометрия, стены)
    const uint32_t Weapon     = 1 << 2; // 4  (Оружие в руках игрока)
    const uint32_t Transparent= 1 << 3; // 8  (Стекла, частицы)
    const uint32_t UI         = 1 << 4; // 16 (Внутриигровой интерфейс)
    
    const uint32_t Everything = 0xFFFFFFFF;
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

struct RenderView
{
    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;
    
    class FrameBuffer* targetFrameBuffer = nullptr;
    
    unsigned int cullingMask = 0xFFFFFFFF;
    bool clearDepthOnly = false;
    float depth = 0.0f; // Приоритет отрисовки
    
    bool drawSkybox = true;
    bool drawGizmos = false;
    bool drawGrid = false;
    
    // Списки команд теперь принадлежат конкретному проходу!
    std::vector<RenderCommand> opaquePass;
    std::vector<RenderCommand> transparentPass;
    
    void submit(const RenderCommand& cmd)
    {
        if (cmd.blendMode == BlendMode::Transparent || cmd.blendMode == BlendMode::Add) {
            transparentPass.push_back(cmd);
        } else {
            opaquePass.push_back(cmd);
        }
    }
};
