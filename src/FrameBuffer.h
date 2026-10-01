//
//  FrameBuffer.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 01.10.26.
//

#pragma once

class FrameBuffer
{
public:
    FrameBuffer();
    
    FrameBuffer(const FrameBuffer&) = delete;             // no copy allowed
    FrameBuffer& operator =(const FrameBuffer&) = delete; // no copy allowed
    
    FrameBuffer(FrameBuffer&& other) noexcept;            // move allowed
    FrameBuffer& operator=(FrameBuffer&& other) noexcept; // move allowed
    
    ~FrameBuffer();
    
public:
    void init();
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
    float m_scale = 2;
    
    // Временное хранилище для старого viewport: { x, y, width, height }
    int m_savedViewport[4] = { 0, 0, 0, 0 };
};
