//
//  Texture.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 21.10.26.
//

#pragma once

class Texture
{
public:
    Texture(unsigned int handle, int width, int height, int channels)
    : m_handle(handle), m_width(width), m_height(height), m_channels(channels) {}
    
    ~Texture();
    
    unsigned int getHandle() const { return m_handle; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    
private:
    unsigned int m_handle = 0;
    int m_width = 0;
    int m_height = 0;
    int m_channels = 0;
};
