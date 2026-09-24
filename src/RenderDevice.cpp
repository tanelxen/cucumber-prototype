//
//  RenderDevice.cpp
//  microwolf
//
//  Created by Fedor Artemenkov on 19.03.26.
//

#include "RenderDevice.h"

#include <stdio.h>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Shader.h"
#include "ShaderVariant.h"
#include "Camera.h"
//#include "Viewport.h"

namespace RenderDevice {
    
    std::vector<RenderCommand> opaquePass;
    std::vector<RenderCommand> transparentPass;
    
    static Shader generic;
    static ShaderVariant flat_shader;
    static ShaderVariant grid_shader;
    
    handle_t whiteTextureId;
    handle_t normalTextureId;
    
//    static Viewport* mainViewport = nullptr;
    
    void init()
    {
        gladLoadGL();
        
        const unsigned char* version = glGetString(GL_VERSION);
        printf("version: %s\n", version);
        
        const unsigned char* device = glGetString(GL_RENDERER);
        printf("device: %s\n", device);
        
//        glEnable(GL_FRAMEBUFFER_SRGB);
        glFrontFace(GL_CCW);
        
        flat_shader.init("assets/shaders/cube_shaded.glsl");
        grid_shader.init("assets/shaders/grid.glsl");
        
        generic.init("assets/shaders/generic.glsl");
        
        {
            unsigned char pixels[4] = { 255, 255, 255, 255 };
            whiteTextureId = makeTexture(1, 1, true, true, pixels);
        }
        
        {
            unsigned char pixels[4] = { 128, 128, 255, 255 };
            normalTextureId = makeTexture(1, 1, true, true, pixels);
        }
        
        
        
//        shCoefficients[0] = glm::vec3(0.43233, 0.219065, 0.161589);
//        shCoefficients[1] = glm::vec3(0.250797, 0.106321, 0.0464793);
//        shCoefficients[2] = glm::vec3(0.30481, 0.187916, 0.128884);
//        shCoefficients[3] = glm::vec3(-0.0314883, -0.0288192, -0.0495037);
//        shCoefficients[4] = glm::vec3(-0.0538252, -0.0303336, -0.0321333);
//        shCoefficients[5] = glm::vec3(0.156495, 0.0838068, 0.0351124);
//        shCoefficients[6] = glm::vec3(0.193687, 0.110605, 0.065372);
//        shCoefficients[7] = glm::vec3(-0.000409677, 0.012437, 0.0138015);
//        shCoefficients[8] = glm::vec3(0.0474989, 0.0719414, 0.0850631);
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
    
    void submit(RenderCommand cmd)
    {
        if (cmd.blendMode == BlendMode::Transparent || cmd.blendMode == BlendMode::Add)
        {
            transparentPass.push_back(cmd);
        }
        else
        {
            opaquePass.push_back(cmd);
        }
    }
    
    void draw(glm::mat4& viewProj, std::vector<RenderCommand>& commands)
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
    
//    void setMainViewport(Viewport* viewport)
//    {
//        mainViewport = viewport;
//    }
    
    void startFrame()
    {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        
        //        glCullFace(GL_FRONT);
//        glCullFace(GL_BACK);
        
        glClearColor(0.01, 0.01, 0.01, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
//        opaquePass.clear();
//        transparentPass.clear();
    }
    
    void commit(Camera* camera)
    {
//        if (mainViewport == nullptr) return;
        if (camera == nullptr) return;
        
//        mainViewport->bind();
        
        glm::mat4x4 viewProj = camera->projection * camera->view;
        
        glDepthMask(GL_TRUE);
        draw(viewProj, opaquePass);
        
        glDepthMask(GL_FALSE);
        draw(viewProj, transparentPass);
        
        glDepthMask(GL_TRUE);
        
//        mainViewport->unbind();
    }
    
    void endFrame()
    {
        opaquePass.clear();
        transparentPass.clear();
    }
}
