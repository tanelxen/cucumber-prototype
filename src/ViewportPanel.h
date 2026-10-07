//
//  ViewportPanel.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#pragma once

#include "EditorPanel.h"

class Viewport;
class Entity;

class ViewportPanel: public EditorPanel
{
public:
    ViewportPanel(const std::string &name);
//    ~Viewport();
    
    void preRender();
    
    void setViewport(Viewport* viewport);
    void setEntity(Entity* entity);
    
    void update(float dt) override;
    void draw() override;
    
public:
    inline bool isHovered() { return m_isHovered; }
    
private:
    Viewport* m_viewport = nullptr;
    Entity* m_entity = nullptr;
    
    class Camera* m_pCamera;
    class CameraController* m_pCameraController;
    
    bool m_isHovered;
    
    float m_pendingWidth = 0;
    float m_pendingHeight = 0;
    bool m_isResizing = false;
};
