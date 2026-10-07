//
//  CameraComponent.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 12.09.26.
//

#pragma once

#include "Component.h"
#include "Viewport.h"
#include "Camera.h"
#include "World.h"

struct CameraComponent : public Component
{
    CameraComponent();
    
    Camera m_camera;
    bool m_isCurrent = true;
    
    bool clearDepthOnly = false;        // Нужно для оверлей-камер (оружие)
    uint32_t cullingMask = 0xFFFFFFFF;  // Какие слои рендерить
    float depth = 0.0f;                 // Очередь отрисовки (чем больше, тем позже рисуется)
    
    float fov = 55.0f;
    
    bool isOverlay = false;
    
    const std::string getTypeName() const override { return "CameraComponent"; }
    
    void save(nlohmann::json& j) const override
    {
        j["fov"] = fov;
    }
    
    void load(const nlohmann::json& j) override
    {
        if(j.contains("fov"))
        {
            fov = j.value("fov", 55.f);
        }
    }
    
    void update(float dt) override;
    
    void gizmo() override;
};
