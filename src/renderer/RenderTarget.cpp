//
//  RenderTarget.cpp
//  Cucumber
//

#include "RenderTarget.h"

#include <glad/glad.h>

#include <algorithm>
#include <cstdio>
#include <utility>

static int ScaledSize(int logical, float scale)
{
    return std::max(1, static_cast<int>(logical * scale));
}

RenderTarget::RenderTarget(int width, int height, float scale)
    : m_width(std::max(1, width)), m_height(std::max(1, height)), m_scale(scale > 0.0f ? scale : 1.0f)
{
    create();
}

RenderTarget::~RenderTarget()
{
    destroy();
}

RenderTarget::RenderTarget(RenderTarget&& other) noexcept
{
    *this = std::move(other);
}

RenderTarget& RenderTarget::operator=(RenderTarget&& other) noexcept
{
    if (this == &other) return *this;

    destroy();

    m_fbo = std::exchange(other.m_fbo, 0u);
    m_rbo = std::exchange(other.m_rbo, 0u);
    m_texture = std::exchange(other.m_texture, 0u);
    m_width = other.m_width;
    m_height = other.m_height;
    m_scale = other.m_scale;
    return *this;
}

int RenderTarget::pixelWidth() const { return ScaledSize(m_width, m_scale); }
int RenderTarget::pixelHeight() const { return ScaledSize(m_height, m_scale); }

bool RenderTarget::readDepth(int x, int y, float& depth) const
{
    if (x < 0 || x >= pixelWidth() || y < 0 || y >= pixelHeight()) {
        return false;
    }

    GLint previousReadFramebuffer = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);
    glReadPixels(x, y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousReadFramebuffer));
    return true;
}

void RenderTarget::create()
{
    glGenFramebuffers(1, &m_fbo);
    glGenTextures(1, &m_texture);
    glGenRenderbuffers(1, &m_rbo);

    reallocate();

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        fprintf(stderr, "[RenderTarget] framebuffer is incomplete\n");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderTarget::reallocate()
{
    const int pw = pixelWidth();
    const int ph = pixelHeight();

    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, pw, ph, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, pw, ph);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

void RenderTarget::destroy()
{
    if (m_fbo) glDeleteFramebuffers(1, &m_fbo);
    if (m_rbo) glDeleteRenderbuffers(1, &m_rbo);
    if (m_texture) glDeleteTextures(1, &m_texture);
    m_fbo = m_rbo = m_texture = 0;
}

void RenderTarget::resize(int width, int height, float scale)
{
    width = std::max(1, width);
    height = std::max(1, height);
    if (scale <= 0.0f) scale = 1.0f;

    const bool pixelsChanged = ScaledSize(width, scale) != pixelWidth() ||
                               ScaledSize(height, scale) != pixelHeight();

    m_width = width;
    m_height = height;
    m_scale = scale;

    if (pixelsChanged) reallocate();
}

void RenderTarget::bind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, pixelWidth(), pixelHeight());
    glEnable(GL_FRAMEBUFFER_SRGB);
}

void RenderTarget::unbind()
{
    glDisable(GL_FRAMEBUFFER_SRGB);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
