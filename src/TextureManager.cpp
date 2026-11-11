//
//  TextureManager.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 21.10.26.
//

#include "TextureManager.h"
#include "RenderDevice.h"

#include <glad/glad.h>
#include <iostream>

//#define STB_IMAGE_IMPLEMENTATION
#include "../vendor/stb_image.h"

void TextureManager::init(const std::filesystem::path& assetsRoot)
{
    m_assetsRoot = assetsRoot;
    createFallbackTexture();
}

std::shared_ptr<Texture> TextureManager::findTexture(const std::string& name)
{
    auto it = m_textureCache.find(name);
    
    if (it != m_textureCache.end())
    {
        return it->second;
    }
    
    return nullptr;
}

std::shared_ptr<Texture> TextureManager::loadTexture(const std::string& name)
{
    // 1. Проверяем кэш
    auto cached = findTexture(name);
    if (cached) return cached;
    
    // 2. Если в имени уже есть расширение, пробуем загрузить напрямую
    std::filesystem::path relativePath(name);
    if (relativePath.has_extension())
    {
        auto tex = loadFromFile(m_assetsRoot / relativePath);
        if (tex) {
            m_textureCache[name] = tex;
            return tex;
        }
    }
    else
    {
        // 3. Перебираем возможные расширения, так как TrenchBroom их опускает
        const std::vector<std::string> extensions = { ".png", ".jpg", ".tga", ".jpeg" };
        for (const auto& ext : extensions)
        {
            std::filesystem::path fullPath = m_assetsRoot / relativePath;
            fullPath.replace_extension(ext);
            
            if (std::filesystem::exists(fullPath))
            {
                auto tex = loadFromFile(fullPath);
                if (tex) {
                    m_textureCache[name] = tex;
                    return tex;
                }
            }
        }
    }
    
    std::cout << "[TextureManager] Warning: Texture not found: " << name << std::endl;
    return m_fallbackTexture;
}

std::shared_ptr<Texture> TextureManager::loadFromFile(const std::filesystem::path& path)
{
    int width, height, channels;
    
    unsigned char* data = stbi_load(path.string().c_str(), &width, &height, &channels, 0);
    if (!data)
    {
        return nullptr;
    }
    
    bool hasAlpha = channels == 4;
    auto textureID = RenderDevice::makeTexture(width, height, hasAlpha, true, data);
    
    stbi_image_free(data);
    
    //std::cout << "[TextureManager] Loaded: " << path.filename() << " (" << width << "x" << height << ")" << std::endl;
    return std::make_shared<Texture>(textureID, width, height, channels);
}

void TextureManager::createFallbackTexture()
{
    // Создаем классическую черно-розовую шахматку 2х2 на случай отсутствия ассетов
    int width = 2;
    int height = 2;
    unsigned char light[3] = { 128, 128, 128 };
    unsigned char dark[3]   = { 100, 100, 100 };
    
    unsigned char data[12]; // 4 пикселя * 3 байта (RGB)
    // Пиксель 0,0
    data[0] = light[0]; data[1] = light[1]; data[2] = light[2];
    // Пиксель 1,0
    data[3] = dark[0];  data[4] = dark[1];  data[5] = dark[2];
    // Пиксель 0,1
    data[6] = dark[0];  data[7] = dark[1];  data[8] = dark[2];
    // Пиксель 1,1
    data[9] = light[0]; data[10] = light[1]; data[11] = light[2];
    
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    m_fallbackTexture = std::make_shared<Texture>(textureID, 32, 32, 3);
}
