//
//  Viewport.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 16.07.26.
//

#pragma once
#include <memory>
#include <glm/glm.hpp>

class Camera;
class FrameBuffer;
class RenderView;

class Viewport
{
public:
    // По умолчанию рендерим в FBO
    Viewport(bool isRenderTarget = true);
    ~Viewport() = default;
    
    void init();
    
    void resize(int width, int height);
    
    void bind();
    void unbind();
    
    // Генерирует готовый RenderView для рендеринга сцены
    RenderView createRenderView();
    
    // Математическая задача: Вычисление луча из пикселя мыши в 3D мир
    void screenPointToRay(const glm::vec2& mousePos, glm::vec3& outStart, glm::vec3& outDir) const;
    
    // Геттеры
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    float getAspectRatio() const { return (float)m_width / (float)m_height; }
    int getTexture() const;
    
    void setCamera(Camera* camera);
    Camera* getCamera() { return m_pCurrentCamera; }
    FrameBuffer* getFrameBuffer() { return m_frameBuffer.get(); }
    
    bool drawSkybox = true;
    bool drawGizmos = false;
    bool drawGrid = false;
    
private:
    std::unique_ptr<FrameBuffer> m_frameBuffer;
    Camera* m_pCurrentCamera = nullptr;
    
    bool m_isRenderTarget = false;
    int m_width = 1280;
    int m_height = 720;
};

