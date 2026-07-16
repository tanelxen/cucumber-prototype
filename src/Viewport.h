//
//  Viewport.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 16.07.26.
//

#pragma once

class Camera;

class Viewport
{
public:
//    Viewport();
    ~Viewport();
    
    void init();
    void setCamera(Camera* camera);
    void resize(int width, int height);
    
    void bind();
    void unbind();
    void renderUI(const char* windowName);
    
    unsigned int m_texture = 0;
    
    int m_width = 1;
    int m_height = 1;
    
private:
    unsigned int m_fbo = 0;
    unsigned int m_rbo = 0;
    
    Camera* m_camera;
};
