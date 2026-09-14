//
//  Viewport.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 16.07.26.
//

#include "Viewport.h"
#include "Camera.h"
#include "Input.h"

#include <glad/glad.h>
#include <imgui.h>

//Viewport::Viewport(int width, int height, Camera* camera) : m_width(width), m_height(height), m_camera(camera)
//{
//}

Viewport::~Viewport()
{
    glDeleteFramebuffers(1, &m_fbo);
    glDeleteTextures(1, &m_texture);
}

void Viewport::init()
{
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_width, m_height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);
    
    glGenRenderbuffers(1, &m_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_width, m_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_rbo);
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Viewport::setCamera(Camera *camera)
{
    m_camera = camera;
    m_camera->setAspectRatio((float)m_width / (float)m_height);
}

void Viewport::updateBounds(float minX, float minY, float maxX, float maxY)
{
    minBounds.x = minX;
    minBounds.y = minY;
    
    maxBounds.x = maxX;
    maxBounds.y = maxY;
    
    int width = int(maxX - minX);
    int height = int(maxY - minY);
    
    if (m_width != width || m_height != height)
    {
        resize(width, height);
    }
}

void Viewport::resize(int width, int height)
{
    m_width = width;
    m_height = height;
    
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width * m_scale, height * m_scale, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    
    glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width * m_scale, height * m_scale);
    
    m_camera->setAspectRatio((float)width / (float)height);
}

void Viewport::bind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width * m_scale, m_height * m_scale);
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_FRAMEBUFFER_SRGB);
//    glDisable(GL_FRAMEBUFFER_SRGB);
}

void Viewport::unbind()
{
    glDisable(GL_FRAMEBUFFER_SRGB);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

glm::vec2 Viewport::getMousePosition()
{
    float sizeX = maxBounds.x - minBounds.x;
    float sizeY = maxBounds.y - minBounds.y;
    
    float x = (Input::getMousePositionX() - minBounds.x) / sizeX;
    float y = (Input::getMousePositionY() - minBounds.y) / sizeY;
    
    return {x, y};
}


