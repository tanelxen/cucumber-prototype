//
//  CameraComponent.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 12.09.26.
//

#include "CameraComponent.h"
#include "TransformComponent.h"
#include "Entity.h"

#include "GizmoRender.h"

CameraComponent::CameraComponent() : Component(Flags::RequiresUpdate)
{
    
}

void CameraComponent::update(float dt)
{
    auto modelMatrix = owner->getTransform().getWorldMatrix();
    m_camera.view = glm::inverse(modelMatrix);
    m_camera.fov = fov;
    
    /*
    // --- АВТОМАТИЧЕСКОЕ ОБНОВЛЕНИЕ РАЗМЕРОВ ---
    // Камера сама запрашивает соотношение сторон у своего вьюпорта!
    if (m_pTargetViewport)
    {
        float aspect = (float)m_pTargetViewport->getWidth() / (float)m_pTargetViewport->getHeight();
        m_camera.setAspectRatio(aspect);
    }
     */
}

void CameraComponent::gizmo()
{
    const glm::mat4& worldMatrix = owner->getTransform().getWorldMatrix();
    glm::vec3 worldPos = glm::vec3(worldMatrix[3]);
    
    GizmoRender::Get().DrawCamera(worldMatrix, 32.f, 32.f, {0, 1, 0, 1});
}

//class DirectionalLightComponent : public Component
//{
//public:
//    std::unique_ptr<FrameBuffer> m_shadowBuffer; // Специфичный FBO (только текстура глубины, без цвета)
//    glm::mat4 m_lightView;
//    glm::mat4 m_lightProjection;
//    
//    void start() override {
//        // Создаем FBO для теней размером, например, 2048x2048
//        // Важно: внутри FrameBuffer для теней не должно включаться sRGB!
//        m_shadowBuffer = std::make_unique<FrameBuffer>(2048, 2048, false);
//    }
//    
//    void update(float dt) override {
//        // Считаем ортографическую проекцию солнца вокруг игрока
//        glm::vec3 lightPos = owner->getTransform().getPosition();
//        glm::vec3 lightDir = owner->getTransform().getForward();
//        
//        m_lightView = glm::lookAt(lightPos, lightPos + lightDir, glm::vec3(0.0f, 1.0f, 0.0f));
//        m_lightProjection = glm::ortho(-20.0f, 20.0f, -20.0f, 20.0f, 0.1f, 100.0f);
//    }
//};
