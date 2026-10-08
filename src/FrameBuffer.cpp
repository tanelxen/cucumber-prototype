//
//  FrameBuffer.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 01.10.26.
//

#include "FrameBuffer.h"
#include <glad/glad.h>

FrameBuffer::FrameBuffer() { }

// Move-конструктор
FrameBuffer::FrameBuffer(FrameBuffer&& other) noexcept
: m_fbo(other.m_fbo), m_rbo(other.m_rbo), m_texture(other.m_texture),
m_width(other.m_width), m_height(other.m_height), m_scale(other.m_scale)
{
    // зануляем старый объект, чтобы его деструктор не удалил ресурсы
    other.m_fbo = 0;
    other.m_rbo = 0;
    other.m_texture = 0;
}

// Move-оператор присваивания
FrameBuffer& FrameBuffer::operator=(FrameBuffer&& other) noexcept
{
    if (this != &other) // Защита от присваивания самому себе (напр. fb = std::move(fb);)
    {
        // 1. Сначала освобождаем СВОИ старые ресурсы
        if (m_fbo) glDeleteFramebuffers(1, &m_fbo);
        if (m_rbo) glDeleteRenderbuffers(1, &m_rbo);
        if (m_texture) glDeleteTextures(1, &m_texture);
        
        // 2. Копируем данные из other
        m_fbo = other.m_fbo;
        m_rbo = other.m_rbo;
        m_texture = other.m_texture;
        m_width = other.m_width;
        m_height = other.m_height;
        m_scale = other.m_scale;
        
        // 3. Зануляем original-объект
        other.m_fbo = 0;
        other.m_rbo = 0;
        other.m_texture = 0;
    }
    return *this;
}

FrameBuffer::~FrameBuffer()
{
    if (m_fbo) glDeleteFramebuffers(1, &m_fbo);
    if (m_rbo) glDeleteRenderbuffers(1, &m_rbo);
    if (m_texture) glDeleteTextures(1, &m_texture);
}

void FrameBuffer::init()
{
    // Рассчитываем реальный размер с учетом scale
    int realWidth = static_cast<int>(m_width * m_scale);
    int realHeight = static_cast<int>(m_height * m_scale);
    
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    // ИСПРАВЛЕНО: здесь тоже используем реальный размер с учетом m_scale
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, realWidth, realHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);
    
    glGenRenderbuffers(1, &m_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
    // ИСПРАВЛЕНО: здесь тоже используем реальный размер с учетом m_scale
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, realWidth, realHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_rbo);
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FrameBuffer::resize(int width, int height)
{
    m_width = width;
    m_height = height;
    
    int realWidth = static_cast<int>(width * m_scale);
    int realHeight = static_cast<int>(height * m_scale);
    
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, realWidth, realHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    
    glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, realWidth, realHeight);
}

void FrameBuffer::bind()
{
    // 1. Сохраняем текущий viewport перед переключением
    glGetIntegerv(GL_VIEWPORT, m_savedViewport);
    
    // 2. Привязываем наш фреймбуфер
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, static_cast<int>(m_width * m_scale), static_cast<int>(m_height * m_scale));
    
    glEnable(GL_FRAMEBUFFER_SRGB);
}

void FrameBuffer::unbind()
{
    glDisable(GL_FRAMEBUFFER_SRGB);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // 3. Восстанавливаем сохраненный viewport обратно
    glViewport(m_savedViewport[0], m_savedViewport[1], m_savedViewport[2], m_savedViewport[3]);
}

