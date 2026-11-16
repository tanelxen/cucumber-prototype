//
//  Texture.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 21.10.26.
//

#include "Texture.h"
#include <glad/glad.h>

Texture::~Texture()
{
    if (m_handle != 0)
    {
        glDeleteTextures(1, &m_handle);
    }
}
