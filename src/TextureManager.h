//
//  TextureManager.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 21.10.26.
//

#pragma once

#include "Texture.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <filesystem>

class TextureManager
{
public:
    static TextureManager& Get()
    {
        static TextureManager instance;
        return instance;
    }
    
    // Инициализация (создание дефолтных текстур)
    void init(const std::filesystem::path& assetsRoot);
    
    // Ищет текстуру в кэше. Если нет — пытается загрузить с диска.
    std::shared_ptr<Texture> loadTexture(const std::string& name);
    
    // Просто проверяет, загружена ли текстура (без обращения к диску)
    std::shared_ptr<Texture> findTexture(const std::string& name);
    
private:
    TextureManager() = default;
    ~TextureManager() = default;
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;
    
    std::shared_ptr<Texture> loadFromFile(const std::filesystem::path& path);
    void createFallbackTexture();
    
    std::filesystem::path m_assetsRoot;
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_textureCache;
    std::shared_ptr<Texture> m_fallbackTexture;
};
