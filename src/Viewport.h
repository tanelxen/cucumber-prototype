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
    
    int getWidth() { return m_width; }
    int getHeight() { return m_height; }
    int getTexture() { return m_texture; }
    
private:
    unsigned int m_fbo = 0;
    unsigned int m_rbo = 0;
    unsigned int m_texture = 0;
    
    int m_width = 1;
    int m_height = 1;
    
    Camera* m_camera;
};
