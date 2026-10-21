//
//  MapAsset.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 21.10.26.
//

#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <filesystem>
#include <unordered_map>

struct MapPlane {
    glm::vec3 normal;
    float distance;
};

struct MapTextureInfo
{
    std::string textureName;

    glm::vec3 uAxis;
    float uShift;
    
    glm::vec3 vAxis;
    float vShift;
    
    float scaleX = 1.0f;
    float scaleY = 1.0f;
};

struct MapBrushSide {
    MapPlane plane;
    MapTextureInfo texture;
};

struct MapBrush {
    std::vector<MapBrushSide> sides;
};

struct MapEntity {
    std::unordered_map<std::string, std::string> properties;
    std::vector<MapBrush> brushes;
};

struct MapAsset
{
    bool loadFromFile(const std::string& filename);
    
    std::vector<MapEntity> entities;
    std::filesystem::path m_filepath;
    std::filesystem::path m_basepath;
};
