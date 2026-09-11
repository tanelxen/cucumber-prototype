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
    
    const std::string getTypeName() const override { return "CameraComponent"; }
    
    void save(nlohmann::json& j) const override
    {
       
    }
    
    void load(const nlohmann::json& j) override
    {
        
    }
    
    void update(float dt) override;
};
