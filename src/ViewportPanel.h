//
//  ViewportPanel.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#pragma once

#include <string>

class Viewport;

class ViewportPanel
{
public:
    ViewportPanel(const std::string &name);
//    ~Viewport();
    
    void setViewport(Viewport* viewport);
    
    void update(float dt);
    void draw();
    
public:
    inline bool isHovered() { return m_isHovered; }
    
private:
    std::string m_name;
    Viewport* m_viewport;
    
    class Camera* m_pCamera;
    class CameraController* m_pCameraController;
    
    bool m_isHovered;
};
