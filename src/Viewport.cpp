//
//  Viewport.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 16.07.26.
//

#include "Viewport.h"
#include "Camera.h"
#include "FrameBuffer.h"
#include "RenderDevice.h"
#include <glad/glad.h>

Viewport::Viewport(bool isRenderTarget) : m_isRenderTarget(isRenderTarget)
{
    if (m_isRenderTarget) {
        m_frameBuffer = std::make_unique<FrameBuffer>();
    }
}

void Viewport::init()
{
    if (m_isRenderTarget && m_frameBuffer) {
        m_frameBuffer->init();
    }
}

void Viewport::resize(int width, int height)
{
    if (width <= 0 || height <= 0) return;
    
    m_width = width;
    m_height = height;
    
    if (m_isRenderTarget && m_frameBuffer) {
        m_frameBuffer->resize(width, height);
    }
    
    // Если к вьюпорту привязана камера — она мгновенно узнает о новом разрешении
    if (m_pCurrentCamera) {
        m_pCurrentCamera->setAspectRatio(getAspectRatio());
    }
}

void Viewport::setCamera(Camera *camera)
{
    m_pCurrentCamera = camera;
    m_pCurrentCamera->setAspectRatio((float)m_width / (float)m_height);
}

void Viewport::bind()
{
    if (m_isRenderTarget && m_frameBuffer) {
        m_frameBuffer->bind();
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, m_width, m_height);
    }
}

void Viewport::unbind()
{
    if (m_isRenderTarget && m_frameBuffer) {
        m_frameBuffer->unbind();
    }
}

RenderView Viewport::createRenderView()
{
    RenderView view;
    
    // Синхронизируем матрицы камеры перед рендером
    if (m_pCurrentCamera) {
        m_pCurrentCamera->setAspectRatio(getAspectRatio());
        view.viewMatrix = m_pCurrentCamera->view;
        view.projectionMatrix = m_pCurrentCamera->projection;
    }
    
    view.targetFrameBuffer = m_isRenderTarget ? m_frameBuffer.get() : nullptr;
    view.drawSkybox = this->drawSkybox;
    view.drawGrid = this->drawGrid;
    view.drawGizmos = this->drawGizmos;
    
    return view;
}

int Viewport::getTexture() const
{
    return m_isRenderTarget ? m_frameBuffer->getTexture() : 0;
}

void Viewport::screenPointToRay(const glm::vec2& mousePos, glm::vec3& outStart, glm::vec3& outDir) const
{
    if (!m_pCurrentCamera) return;
    
    // 1. Переводим пиксельные координаты мыши в Normalized Device Coordinates (NDC) [-1, 1]
    float x = (2.0f * mousePos.x) / (float)m_width - 1.0f;
    float y = 1.0f - (2.0f * mousePos.y) / (float)m_height; // Инвертируем Y для OpenGL
    
    glm::vec4 rayClip = glm::vec4(x, y, -1.0f, 1.0f);
    
    // 2. Из пространства отсечения (Clip Space) в пространство камеры (Eye Space)
    glm::vec4 rayEye = glm::inverse(m_pCurrentCamera->projection) * rayClip;
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f); // Направляем луч вперед
    
    // 3. Из пространства камеры в мировое пространство (World Space)
    glm::vec3 rayWorld = glm::vec3(glm::inverse(m_pCurrentCamera->view) * rayEye);
    outDir = glm::normalize(rayWorld);
    
    // Точка начала луча — это позиция камеры в мире
    outStart = m_pCurrentCamera->getPosition();
}
