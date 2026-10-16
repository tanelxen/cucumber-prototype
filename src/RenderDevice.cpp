//
//  RenderDevice.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 19.03.26.
//

#include "RenderDevice.h"

#include "Shader.h"
#include "ShaderVariant.h"
#include "Camera.h"

#include "Sky.h"
#include "Grid.h"
#include "SpriteRenderer.h"
#include "DecalManager.h"
#include "GizmoRender.h"
#include "GameUI.h"

#include "Viewport.h"
#include "FrameBuffer.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <vector>

//static Sky g_sky;

// Вообще отрисовку сетки надо перенести в GizmoRenderer, но пока что тут
static Grid g_grid;

namespace RenderDevice {
    
//    std::vector<RenderCommand> opaquePass;
//    std::vector<RenderCommand> transparentPass;
    
    // Хранилище проходов для текущего кадра
    static std::vector<RenderView> registeredViews;
    
    static Shader generic;
    static ShaderVariant flat_shader;
    static ShaderVariant grid_shader;
    
    handle_t whiteTextureId;
    handle_t normalTextureId;
    
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
        
//        g_sky.init();
        g_grid.init();
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
        
        return id;
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
    
    void draw(glm::mat4& viewProj, const std::vector<RenderCommand>& commands)
    {
        handle_t lastVAO = -1;
        handle_t lastIBO = -1;
        handle_t lastProgram = -1;
        handle_t lastBaseTexture = -1;
        handle_t lastLightmap = -1;
        
        for (auto cmd : commands)
        {
            ShaderVariant* shader;
            
            switch (cmd.pipeline)
            {
                case PipelineType::Generic:
                    shader = generic.getVariant(cmd.featureMask);
                    break;
                    
                case PipelineType::FlatShaded:
                    shader = &flat_shader;
                    break;
                    
                case PipelineType::Grid:
                    shader = &grid_shader;
                    break;
            }
            
            if (lastProgram != shader->getProgramId())
            {
                shader->bind();
                lastProgram = shader->getProgramId();
            }
            
            shader->setUniform("uModel", cmd.transform);
            shader->setUniform("uVP", viewProj);
            
            cmd.reverseWinding ? glCullFace(GL_FRONT) : glCullFace(GL_BACK);
            
            switch (cmd.blendMode)
            {
                case BlendMode::Opaque:
                    glEnable(GL_CULL_FACE);
                    glDisable(GL_BLEND);
                    glDisable(GL_POLYGON_OFFSET_FILL);
                    break;
                    
                case BlendMode::Mask:
                    glDisable(GL_CULL_FACE);
                    glDisable(GL_BLEND);
                    glDisable(GL_POLYGON_OFFSET_FILL);
                    break;
                    
                case BlendMode::Add:
                    glDisable(GL_CULL_FACE);
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                    glEnable(GL_POLYGON_OFFSET_FILL);
                    glPolygonOffset(cmd.lightMap == 0 ? -16 : -8, 1);
                    break;
                    
                case BlendMode::Transparent:
                    glDisable(GL_CULL_FACE);
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    glEnable(GL_POLYGON_OFFSET_FILL);
                    glPolygonOffset(cmd.lightMap == 0 ? -16 : -8, 1);
                    break;
            }
            
            if (cmd.featureMask & ShaderFeature::HasAlbedoMap && lastBaseTexture != cmd.baseTexture)
            {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, cmd.baseTexture);
                
                lastBaseTexture = cmd.baseTexture;
            }
            
            if (cmd.featureMask & ShaderFeature::HasLightMap && lastLightmap != cmd.lightMap)
            {
                shader->setUniform("s_lightMap", 1);
                
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, cmd.lightMap);
                
                lastLightmap = cmd.lightMap;
            }
            
            if (cmd.pipeline != PipelineType::Grid)
            {
                shader->setUniform("color", cmd.color);
            }
            
            if (cmd.featureMask & ShaderFeature::HasSkinning && cmd.bones != nullptr)
            {
                shader->setUniform("uBones[0]", *cmd.bones);
            }
            
            if (cmd.featureMask & ShaderFeature::HasSpherical)
            {
                shader->setUniform("u_SH[0]", shCoefficients);
            }
            
            if (lastVAO != cmd.vao)
            {
                glBindVertexArray(cmd.vao);
                lastVAO = cmd.vao;
            }
            
            auto bufferOffset = cmd.start * sizeof(uint32_t);
            glDrawElements(GL_TRIANGLES, cmd.count, GL_UNSIGNED_INT, (void *)bufferOffset);
        }
    }
    
    void submitView(const RenderView& view)
    {
        registeredViews.push_back(view);
    }
    
    void flush()
    {
        if (registeredViews.empty()) return;
        
        // 1. Сортируем зарегистрированные проходы по их depth
        std::sort(registeredViews.begin(), registeredViews.end(), [](const RenderView& a, const RenderView& b) {
            return a.depth < b.depth;
        });
        
        // 2. Прогоняем каждый проход рендера
        for (const auto& view : registeredViews)
        {
            if (view.targetFrameBuffer)
            {
                view.targetFrameBuffer->bind();
            }
            else
            {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }
            
            // Настройка очистки буферов
            if (view.clearDepthOnly)
            {
                glEnable(GL_DEPTH_TEST);
                glClear(GL_DEPTH_BUFFER_BIT);
            }
            else
            {
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(GL_LESS);
                glClearColor(0.01, 0.01, 0.01, 1);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            }
            
            if (view.drawSkybox && !view.clearDepthOnly)
            {
//                g_sky.draw(view.viewMatrix, view.projectionMatrix);
            }
            
            if (view.drawGrid)
            {
                g_grid.draw(view.viewMatrix, view.projectionMatrix);
            }
            
            glm::mat4x4 viewProj = view.projectionMatrix * view.viewMatrix;
            
            // Отрисовываем накопленную геометрию сцены
            glDepthMask(GL_TRUE);
            draw(viewProj, view.opaquePass);
            
            glDepthMask(GL_FALSE);
            draw(viewProj, view.transparentPass);
            glDepthMask(GL_TRUE);
            
            // Отрисовка сопутствующих систем
            SpriteRenderer::Flush(view.viewMatrix, view.projectionMatrix);
            DecalManager::Flush(view.viewMatrix, view.projectionMatrix);
            
            if (view.drawGizmos)
            {
                GizmoRender::Get().Flush(view.viewMatrix, view.projectionMatrix);
            }
            
            // FIXME: Resize должен быть не тут
            if (view.cullingMask & Layers::UI)
            {
                int width = view.targetFrameBuffer->getWidth();
                int height = view.targetFrameBuffer->getHeight();
                GameUI::Resize(width, height);
                GameUI::Flush();
            }
            
            if (view.targetFrameBuffer)
            {
                view.targetFrameBuffer->unbind();
            }
        }
        
        // Возвращаем дефолтный буфер и очищаем команды
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        registeredViews.clear();
        
        GizmoRender::Get().Clear();
        SpriteRenderer::Clear();
        GameUI::Clear();
    }
}
