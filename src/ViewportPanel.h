//
//  ViewportPanel.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#pragma once

#include "EditorPanel.h"
#include <glm/glm.hpp>

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
    void setEditorCameraPosition(const glm::vec3& position);
    glm::vec3 getEditorCameraPosition() const;
    
    void update(float dt) override;
    void draw() override;
    
public:
    inline bool isHovered() { return m_isHovered; }
    
private:
    Viewport* m_viewport = nullptr;
    Entity* m_entity = nullptr;
    
    class Camera* m_pCamera;
    class CameraController* m_pCameraController;
    
    bool m_isHovered = false;
    bool m_isGameplayInputSource = false;
    bool m_hasImageBounds = false;
    float m_imageLeft = 0;
    float m_imageTop = 0;
    float m_imageRight = 0;
    float m_imageBottom = 0;
    
    float m_pendingWidth = 0;
    float m_pendingHeight = 0;
    bool m_isResizing = false;
};
