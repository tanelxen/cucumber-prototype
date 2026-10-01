//
//  Viewport.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 16.07.26.
//

#pragma once
#include <memory>

class Camera;
class FrameBuffer;

class Viewport
{
public:
    Viewport();
    ~Viewport() = default;
    
    void init();
    void setCamera(Camera* camera);
    void resize(int width, int height);
//    void updateBounds(float minX, float minY, float maxX, float maxY);
    
    void bind();
    void unbind();
    
    int getWidth() { return m_width; }
    int getHeight() { return m_height; }
    int getTexture();
    
    Camera* getCamera() { return m_camera; }
    
//    glm::vec2 getMousePosition();
    
private:
    std::unique_ptr<FrameBuffer> m_frameBuffer;
    
    int m_width = 1;
    int m_height = 1;
    
//    glm::vec2 minBounds;
//    glm::vec2 maxBounds;
    
    Camera* m_camera;
};
