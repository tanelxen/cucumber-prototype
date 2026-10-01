//
//  Viewport.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 16.07.26.
//

#include "Viewport.h"
#include "FrameBuffer.h"
#include "Camera.h"

Viewport::Viewport()
{
    m_frameBuffer = std::make_unique<FrameBuffer>();
}

void Viewport::init()
{
    m_frameBuffer->init();
}

int Viewport::getTexture() { return m_frameBuffer->getTexture(); }

void Viewport::setCamera(Camera *camera)
{
    m_camera = camera;
    m_camera->setAspectRatio((float)m_width / (float)m_height);
}

//void Viewport::updateBounds(float minX, float minY, float maxX, float maxY)
//{
//    minBounds.x = minX;
//    minBounds.y = minY;
//    
//    maxBounds.x = maxX;
//    maxBounds.y = maxY;
//    
//    int width = int(maxX - minX);
//    int height = int(maxY - minY);
//    
//    if (m_width != width || m_height != height)
//    {
//        resize(width, height);
//    }
//}

void Viewport::resize(int width, int height)
{
    m_camera->setAspectRatio((float)width / (float)height);
    m_frameBuffer->resize(width, height);
    
    m_height = height;
    m_width = width;
}

void Viewport::bind()
{
    m_frameBuffer->bind();
}

void Viewport::unbind()
{
    m_frameBuffer->unbind();
}

//glm::vec2 Viewport::getMousePosition()
//{
//    float sizeX = maxBounds.x - minBounds.x;
//    float sizeY = maxBounds.y - minBounds.y;
//    
//    float x = (Input::getMousePositionX() - minBounds.x) / sizeX;
//    float y = (Input::getMousePositionY() - minBounds.y) / sizeY;
//    
//    return {x, y};
//}
