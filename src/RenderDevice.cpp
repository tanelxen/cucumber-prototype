//
//  RenderDevice.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 19.03.26.
//
//  Этап 3 + очистка + этап 4 (тени вынесены в ShadowSystem):
//   - виды из пула, сортировка команд по ключу;
//   - GLState: нет избыточных gl-вызовов и нет glGet*/glIsEnabled;
//   - per-view uniform'ы (VP, позиция камеры, тени) ставятся один раз при смене программы, а не на каждую команду;
//   - uniform'ы по location, шейдер выбирается только при смене (pipeline, features);
//   - сэмплеры привязаны к юнитам при линковке (исправляет баг с s_lightMap).
//

#include "RenderDevice.h"

#include "GLState.h"
#include "RenderTarget.h"
#include "ShadowSystem.h"

#include "Shader.h"
#include "ShaderVariant.h"

#include "SpriteRenderer.h"
#include "DecalManager.h"
#include "GizmoRender.h"
#include "renderer/OverlayRender.h"
#include "GameUI.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <numeric>
#include <vector>

namespace RenderDevice {
    
    // Пул видов: между кадрами сохраняет ёмкость векторов команд
    static std::vector<RenderView> viewPool;
    static size_t viewCount = 0;
    static std::vector<uint32_t> viewOrder;
    
    static Shader generic;
    static ShaderVariant flat_shader;
    static ShaderVariant grid_shader;
    
    handle_t whiteTextureId;
    handle_t normalTextureId;
    
    static GLuint lightingUBO = 0;
    struct UBOLightData {
        glm::vec4 position;  // w = type (0=dir, 1=point)
        glm::vec4 direction; // w = range
        glm::vec4 color;     // w = intensity
        glm::vec4 parameters; // x = spot angle in degrees
    };
    
    struct UBOLightingBlock {
        UBOLightData lights[MaxLightsPerView];
        int lightCount;
        float padding[3];    // Выравнивание по std140 (16 байт)
    };
    
    // Всё, что нужно проходу отрисовки и одинаково для всех его команд
    struct PassContext {
        glm::mat4 viewProj;
        glm::vec3 cameraPos;
        bool hasShadows;
        std::array<glm::mat4, 4> lightSpace;   // [0] — солнце вида, [1..3] — прожекторы
    };
    
    void init()
    {
        gladLoadGL();
        
        const unsigned char* version = glGetString(GL_VERSION);
        printf("version: %s\n", version);
        
        const unsigned char* device = glGetString(GL_RENDERER);
        printf("device: %s\n", device);
        
        glFrontFace(GL_CCW);
        
        flat_shader.init("resources/shaders/cube_shaded.glsl");
        grid_shader.init("resources/shaders/grid.glsl");
        
        generic.init("resources/shaders/generic.glsl");
        
        {
            unsigned char pixels[4] = { 255, 255, 255, 255 };
            whiteTextureId = makeTexture(1, 1, true, true, pixels);
        }
        
        {
            unsigned char pixels[4] = { 128, 128, 255, 255 };
            normalTextureId = makeTexture(1, 1, true, true, pixels);
        }
        
        glGenBuffers(1, &lightingUBO);
        glBindBuffer(GL_UNIFORM_BUFFER, lightingUBO);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(UBOLightingBlock), nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, 1, lightingUBO); // Привязываем к binding point = 1
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        
        GLState::invalidate();
    }
    
    handle_t getWhiteTexture() { return whiteTextureId; }
    
    handle_t makeTexture(int width, int height, bool hasAlpha, bool standard, const void* data)
    {
        GLuint id;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        
        GLenum internalformat;
        
        if (standard) {
            internalformat = hasAlpha ? GL_SRGB_ALPHA : GL_SRGB;
        } else {
            internalformat = hasAlpha ? GL_RGBA : GL_RGB;
        }
        
        GLenum format = hasAlpha ? GL_RGBA : GL_RGB;
        
        glTexImage2D(GL_TEXTURE_2D, 0, internalformat, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        
        GLfloat maxAnisotropy = 0.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAnisotropy);
        
        glBindTexture(GL_TEXTURE_2D, 0);
        
        GLState::invalidate(); // привязки текстур изменены мимо кэша
        
        return id;
    }
    
    void deleteTexture(handle_t handle)
    {
        GLuint id = handle;
        glDeleteTextures(1, &id);
        GLState::invalidate();   // кэш привязок мог хранить это имя, а GL переиспользует имена
    }
    
    handle_t makeVertexBuffer(long size, void* data)
    {
        GLuint id;
        
        glGenBuffers(1, &id);
        glBindBuffer(GL_ARRAY_BUFFER, id);
        glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        return id;
    }
    
    handle_t makeIndexBuffer(long size, void* data)
    {
        GLuint id;
        
        glGenBuffers(1, &id);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        
        return id;
    }
    
    // -----------------------------------------------------------------------------------------
    // Основной проход
    // -----------------------------------------------------------------------------------------
    
    static ShaderVariant* resolveShader(const RenderCommand& cmd, bool hasShadows)
    {
        switch (cmd.pipeline)
        {
            case PipelineType::Generic:
                return generic.getVariant(cmd.featureMask | (hasShadows ? ShaderFeature::HasShadows : 0));
                
            case PipelineType::FlatShaded:
                return &flat_shader;
                
            case PipelineType::Grid:
                return &grid_shader;
        }
        return nullptr;
    }
    
    // Uniform'ы, одинаковые для всех команд вида. Значения живут в программе, поэтому достаточно
    // выставить их один раз после выбора программы.
    static void applyViewUniforms(const ShaderVariant& shader, const PassContext& ctx, const RenderCommand& cmd)
    {
        const ShaderVariant::Locations& loc = shader.locations();
        
        shader.setUniformAt(loc.vp, ctx.viewProj);
        shader.setUniformAt(loc.viewPos, ctx.cameraPos);
        
        // Слот тени каждого источника приходит в UBO (LightData.parameters.y), здесь только матрицы 2D-карт
        if (ctx.hasShadows && cmd.pipeline == PipelineType::Generic)
        {
            shader.setUniformAt(loc.lightSpaceMatrices, ctx.lightSpace.data(), static_cast<int>(ctx.lightSpace.size()));
        }
        
        if (cmd.featureMask & ShaderFeature::HasSpherical)
        {
            shader.setUniformAt(loc.sh, shCoefficients.data(), static_cast<int>(shCoefficients.size()));
        }

    }
    
    static void applyRenderState(const RenderCommand& cmd)
    {
        GLState::cullFace(cmd.reverseWinding ? GL_FRONT : GL_BACK);
        
        switch (cmd.blendMode)
        {
            case BlendMode::Opaque:
                GLState::cull(true);
                GLState::blend(false);
                GLState::polygonOffset(false);
                break;
                
            case BlendMode::Mask:
                GLState::cull(false);
                GLState::blend(false);
                GLState::polygonOffset(false);
                break;
                
            case BlendMode::Add:
                GLState::cull(false);
                GLState::blend(true);
                GLState::blendFunc(GL_SRC_ALPHA, GL_ONE);
                GLState::polygonOffset(true, cmd.lightMap == 0 ? -16.0f : -8.0f, 1.0f);
                break;
                
            case BlendMode::Transparent:
                GLState::cull(false);
                GLState::blend(true);
                GLState::blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                GLState::polygonOffset(true, cmd.lightMap == 0 ? -16.0f : -8.0f, 1.0f);
                break;
        }
    }
    
    // Команды уже отсортированы (RenderView::sortCommands): одинаковые программы/текстуры/VAO идут подряд.
    static void drawPass(const PassContext& ctx, const std::vector<RenderCommand>& commands)
    {
        ShaderVariant* shader = nullptr;
        PipelineType lastPipeline = PipelineType::Generic;
        FeatureMask lastMask = 0;
        
        unsigned int lastProgram = 0xFFFFFFFFu;
        glm::mat4 lastTransform;
        glm::vec4 lastColor;
        bool transformValid = false;
        bool colorValid = false;
        
        for (const RenderCommand& cmd : commands)
        {
            // Шейдер переопределяем только при смене (pipeline, features)
            if (!shader || cmd.pipeline != lastPipeline || cmd.featureMask != lastMask)
            {
                shader = resolveShader(cmd, ctx.hasShadows);
                if (!shader || !shader->isValid())
                {
                    shader = nullptr;
                    continue;
                }
                lastPipeline = cmd.pipeline;
                lastMask = cmd.featureMask;
                
                if (shader->getProgramId() != lastProgram)
                {
                    lastProgram = shader->getProgramId();
                    GLState::useProgram(lastProgram);
                    transformValid = false;
                    colorValid = false;
                }
                
                applyViewUniforms(*shader, ctx, cmd);
            }
            
            const ShaderVariant::Locations& loc = shader->locations();
            const float alphaCutoff = cmd.blendMode == BlendMode::Mask ? 0.5f : 0.0f;
            shader->setUniformAt(loc.alphaCutoff, alphaCutoff);
            
            if (!transformValid || std::memcmp(&lastTransform, &cmd.transform, sizeof(glm::mat4)) != 0)
            {
                shader->setUniformAt(loc.model, cmd.transform);
                lastTransform = cmd.transform;
                transformValid = true;
            }
            
            applyRenderState(cmd);
            
            if (cmd.featureMask & ShaderFeature::HasAlbedoMap)
            {
                GLState::bindTexture2D(0, cmd.baseTexture);
            }
            
            if (cmd.featureMask & ShaderFeature::HasLightMap)
            {
                GLState::bindTexture2D(1, cmd.lightMap);
            }
            
            if (cmd.pipeline != PipelineType::Grid && (!colorValid || cmd.color != lastColor))
            {
                shader->setUniformAt(loc.color, cmd.color);
                lastColor = cmd.color;
                colorValid = true;
            }
            
            if ((cmd.featureMask & ShaderFeature::HasSkinning) && cmd.bones != nullptr)
            {
                shader->setUniformAt(loc.bones, *cmd.bones);
            }
            
            GLState::bindVAO(cmd.vao);
            
            const auto bufferOffset = cmd.start * sizeof(uint32_t);
            glDrawElements(GL_TRIANGLES, cmd.count, GL_UNSIGNED_INT, (void*)bufferOffset);
        }
    }
    
    // -----------------------------------------------------------------------------------------
    // Кадр
    // -----------------------------------------------------------------------------------------
    
    RenderView& acquireView(const ViewDesc& desc)
    {
        if (viewCount == viewPool.size())
        {
            viewPool.emplace_back();
        }
        
        RenderView& view = viewPool[viewCount++];
        view.reset(desc);
        return view;
    }
    
    static void uploadLights(const RenderView& view)
    {
        UBOLightingBlock uboBlock{};
        uboBlock.lightCount = std::min((int)view.lights.size(), (int)MaxLightsPerView);
        
        for (int i = 0; i < uboBlock.lightCount; i++) {
            const auto& src = view.lights[i];
            uboBlock.lights[i].position  = glm::vec4(src.position, (float)src.type);
            uboBlock.lights[i].direction = glm::vec4(src.direction, src.range);
            uboBlock.lights[i].color     = glm::vec4(src.color, src.intensity);
            uboBlock.lights[i].parameters = glm::vec4(glm::cos(glm::radians(src.spotAngle)),
                                                      static_cast<float>(src.shadowSlot),
                                                      src.bakedStatic ? 1.0f : 0.0f, 0.0f);
        }
        
        glBindBuffer(GL_UNIFORM_BUFFER, lightingUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(UBOLightingBlock), &uboBlock);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }
    
    static void renderView(RenderView& view, ShadowSystem& shadows)
    {
        view.sortCommands();
        
        // ---- Тени ----
        // Точечные и прожекторные карты уже готовы (ShadowSystem::renderQueued, раз за кадр и с кэшем).
        // Здесь только солнце: оно зависит от камеры этого вида. Оверлейные виды берут уже готовую карту.
        const bool hasShadows = shadows.ready() && std::any_of(view.lights.begin(), view.lights.end(),
            [](const GPUTimeLight& l) { return l.shadowSlot >= 0; });
        
        shadows.renderSun(view.sun);
        
        // ---- Цель ----
        const ViewDesc& desc = view.desc;
        
        if (desc.target)
        {
            desc.target->bind();
        }
        else
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, desc.size.x, desc.size.y);
        }
        GLState::invalidate(); // RenderTarget::bind работает мимо кэша
        
        // ---- Очистка ----
        GLState::depthTest(true);
        GLState::depthFunc(GL_LESS);
        GLState::depthMask(true);
        
        switch (desc.clear)
        {
            case ClearMode::ColorDepth:
                glClearColor(desc.clearColor.x, desc.clearColor.y, desc.clearColor.z, desc.clearColor.w);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                break;
                
            case ClearMode::Depth:
                glClear(GL_DEPTH_BUFFER_BIT);
                break;
                
            case ClearMode::None:
                break;
        }
        
        if ((desc.flags & ViewFlag::Sky) && desc.clear == ClearMode::ColorDepth)
        {
//            g_sky.draw(desc.camera.view, desc.camera.proj);
        }
        
        if (desc.flags & ViewFlag::Grid)
        {
            GizmoRender::Get().DrawGrid(desc.camera.view, desc.camera.proj, desc.camera.ortho, desc.gridStep, desc.size);
            GLState::invalidate();
        }
        
        // ---- Сцена ----
        uploadLights(view);
        
        PassContext ctx;
        ctx.viewProj = desc.camera.viewProj();
        ctx.cameraPos = desc.camera.position;
        ctx.hasShadows = hasShadows;
        ctx.lightSpace = shadows.lightSpaceMatrices(view.sun);
        
        if (hasShadows)
        {
            shadows.bindForSampling();
        }
        
        GLState::depthMask(true);
        drawPass(ctx, view.opaquePass);
        
        GLState::depthMask(false);
        drawPass(ctx, view.transparentPass);
        GLState::depthMask(true);
        
        // ---- Сопутствующие системы (мимо GLState) ----
        SpriteRenderer::Flush(desc.camera.view, desc.camera.proj);
        DecalManager::Flush(desc.camera.view, desc.camera.proj);
        
        if (desc.overlayLines != 0)
        {
            OverlayRender::Get().DrawLineSet(desc.overlayLines, desc.camera.view, desc.camera.proj, desc.size, desc.overlayLinesDepthTest, desc.overlayLinesAlpha);
        }
        
        if (desc.flags & ViewFlag::Debug)
        {
            GizmoRender::Get().Flush(desc.camera.view, desc.camera.proj);
        }
        
        if (desc.flags & ViewFlag::EditOverlay)
        {
            OverlayRender::Get().Flush(desc.camera.view, desc.camera.proj, desc.size);
        }
        
        // FIXME: Resize должен быть не тут
        if (desc.flags & ViewFlag::GameUI)
        {
            GameUI::Resize(desc.size.x, desc.size.y);
            GameUI::Flush();
        }
        
        GLState::invalidate();
        
        if (desc.target)
        {
            desc.target->unbind();
        }
    }
    
    void flush(ShadowSystem& shadows)
    {
        if (viewCount > 0)
        {
            // Порядок видов: стабильная сортировка индексов (RenderView тяжёлые, их не двигаем)
            viewOrder.resize(viewCount);
            std::iota(viewOrder.begin(), viewOrder.end(), 0u);
            std::stable_sort(viewOrder.begin(), viewOrder.end(), [](uint32_t a, uint32_t b) {
                return viewPool[a].desc.order < viewPool[b].desc.order;
            });
            
            GLState::invalidate();
            
            for (uint32_t index : viewOrder)
            {
                renderView(viewPool[index], shadows);
            }
            
            // Возвращаем дефолтный буфер, виды возвращаются в пул
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            GLState::invalidate();
            viewCount = 0;
        }
        
        // Очереди чистим ВСЕГДА, даже когда видов нет (все панели свёрнуты): иначе гизмо и спрайты копились бы вечно
        GizmoRender::Get().Clear();
        OverlayRender::Get().Clear();
        SpriteRenderer::Clear();
        GameUI::Clear();
    }
}
