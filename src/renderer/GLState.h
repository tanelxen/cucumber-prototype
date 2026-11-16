//
//  GLState.h
//  Cucumber
//
//  Кэш состояния OpenGL. Пропускает только избыточные вызовы: если значение в кэше известно и равно
//  запрошенному, glXxx не вызывается. Неизвестное состояние (после чужого кода) = всегда вызываем.
//
//  ПРАВИЛО: любой код, меняющий GL-состояние мимо GLState (Grid, Sprite/Decal/Gizmo, GameUI, ImGui,
//  RenderTarget::bind), должен сопровождаться вызовом GLState::invalidate().
//

#pragma once

#include <glad/glad.h>
#include <cstdint>

namespace GLState
{
    constexpr GLuint kUnknown = 0xFFFFFFFFu;
    constexpr int kTextureUnits = 8;

    struct Cache
    {
        int8_t cull = -1, blend = -1, depthTest = -1, depthMask = -1, polygonOffset = -1, srgb = -1;
        GLenum cullFace = 0, depthFunc = 0, blendSrc = 0, blendDst = 0;
        float offsetFactor = 0.0f, offsetUnits = 0.0f;

        GLuint program = kUnknown, vao = kUnknown, framebuffer = kUnknown, activeUnit = kUnknown;
        GLint viewport[4] = { -1, -1, -1, -1 };
        GLuint tex2D[kTextureUnits];

        Cache() { for (auto& t : tex2D) t = kUnknown; }
    };

    inline Cache g;

    inline void invalidate() { g = Cache{}; }

    inline void setCap(GLenum cap, int8_t& cached, bool on)
    {
        const int8_t value = on ? 1 : 0;
        if (cached == value) return;
        if (on) glEnable(cap); else glDisable(cap);
        cached = value;
    }

    inline void cull(bool on)          { setCap(GL_CULL_FACE, g.cull, on); }
    inline void blend(bool on)         { setCap(GL_BLEND, g.blend, on); }
    inline void depthTest(bool on)     { setCap(GL_DEPTH_TEST, g.depthTest, on); }
    inline void framebufferSRGB(bool on) { setCap(GL_FRAMEBUFFER_SRGB, g.srgb, on); }

    inline void depthMask(bool on)
    {
        const int8_t value = on ? 1 : 0;
        if (g.depthMask == value) return;
        glDepthMask(on ? GL_TRUE : GL_FALSE);
        g.depthMask = value;
    }

    inline void cullFace(GLenum face)
    {
        if (g.cullFace == face) return;
        glCullFace(face);
        g.cullFace = face;
    }

    inline void depthFunc(GLenum func)
    {
        if (g.depthFunc == func) return;
        glDepthFunc(func);
        g.depthFunc = func;
    }

    inline void blendFunc(GLenum src, GLenum dst)
    {
        if (g.blendSrc == src && g.blendDst == dst) return;
        glBlendFunc(src, dst);
        g.blendSrc = src;
        g.blendDst = dst;
    }

    // on=false: выключаем смещение; значения factor/units тогда не важны.
    inline void polygonOffset(bool on, float factor = 0.0f, float units = 0.0f)
    {
        setCap(GL_POLYGON_OFFSET_FILL, g.polygonOffset, on);
        if (!on) return;
        if (g.offsetFactor == factor && g.offsetUnits == units) return;
        glPolygonOffset(factor, units);
        g.offsetFactor = factor;
        g.offsetUnits = units;
    }

    inline void bindFramebuffer(GLuint fbo)
    {
        if (g.framebuffer == fbo) return;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        g.framebuffer = fbo;
    }

    inline void viewport(GLint x, GLint y, GLint w, GLint h)
    {
        if (g.viewport[0] == x && g.viewport[1] == y && g.viewport[2] == w && g.viewport[3] == h) return;
        glViewport(x, y, w, h);
        g.viewport[0] = x; g.viewport[1] = y; g.viewport[2] = w; g.viewport[3] = h;
    }

    inline void useProgram(GLuint program)
    {
        if (g.program == program) return;
        glUseProgram(program);
        g.program = program;
    }

    inline void bindVAO(GLuint vao)
    {
        if (g.vao == vao) return;
        glBindVertexArray(vao);
        g.vao = vao;
    }

    inline void activeTexture(GLuint unit)
    {
        if (g.activeUnit == unit) return;
        glActiveTexture(GL_TEXTURE0 + unit);
        g.activeUnit = unit;
    }

    inline void bindTexture2D(GLuint unit, GLuint texture)
    {
        if (unit < GLuint(kTextureUnits))
        {
            if (g.tex2D[unit] == texture) return;
            g.tex2D[unit] = texture;
        }
        activeTexture(unit);
        glBindTexture(GL_TEXTURE_2D, texture);
    }

    // Для не-2D целей (shadow array, cube array). Привязку не кэшируем: их биндят раз на вид.
    inline void bindTextureRaw(GLuint unit, GLenum target, GLuint texture)
    {
        activeTexture(unit);
        glBindTexture(target, texture);
    }
}
