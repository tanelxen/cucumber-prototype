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
    
    void save(nlohmann::json& j) const override
    {
        j["enabled"] = enabled;
        j["primary"] = m_isPrimary;
        j["overlay"] = m_isOverlay;
        j["cameraStackId"] = cameraStackId;
        
        j["clearDepthOnly"] = clearDepthOnly;
        j["cullingMask"] = cullingMask;
        j["depth"] = depth;
        
        j["fov"] = fov;
    }
    
    void load(const nlohmann::json& j) override
    {
        if (j.contains("enabled"))
        {
            enabled = j.at("enabled");
        }
        else if (j.contains("current"))
        {
            enabled = j.at("current");
        }
        
        if(j.contains("clearDepthOnly"))
        {
            clearDepthOnly = j.at("clearDepthOnly");
        }

        if (j.contains("overlay"))
        {
            m_isOverlay = j.at("overlay");
        }
        else if (j.contains("current"))
        {
            m_isOverlay = clearDepthOnly;
        }

        if (j.contains("primary"))
        {
            m_isPrimary = j.at("primary");
        }

        if (j.contains("cameraStackId"))
        {
            cameraStackId = j.at("cameraStackId");
        }
        
        if(j.contains("cullingMask"))
        {
            cullingMask = j.at("cullingMask");
        }
        
        if(j.contains("depth"))
        {
            depth = j.at("depth");
        }
        
        if(j.contains("fov"))
        {
            fov = j.value("fov", 55.f);
        }
    }
    
    void gizmo() override;
};
