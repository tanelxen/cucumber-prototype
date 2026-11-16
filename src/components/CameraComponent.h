//
//  CameraComponent.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 12.09.26.
//

#pragma once

#include "Component.h"

class RenderTarget;

struct CameraComponent : public Component
{
    CameraComponent();
    
    bool m_isPrimary = false;
    bool m_isOverlay = false;
    uint32_t cameraStackId = 0;
    
    RenderTarget* m_renderTarget = nullptr;
    
    bool clearDepthOnly = false;        // Сохраняет цвет предыдущего вида, очищая depth для оверлея.
    uint32_t cullingMask = 0xFFFFFFFF;  // Какие слои рендерить, например только слой оружия.
    float depth = 0.0f;                 // Стек камер: меньшее значение рендерится раньше.
    
    float fov = 55.0f;
    
    const std::string getTypeName() const override { return "CameraComponent"; }
    
    void save(nlohmann::json& j) const override;
    void load(const nlohmann::json& j) override;
    
    void gizmo() override;
};
