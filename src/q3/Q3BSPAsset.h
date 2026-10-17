#pragma once

#include <vector>
#include <string>
#include <filesystem>

#include "Q3BSPTypes.h"

struct Q3BSPAsset
{
    bool initFromFile(const std::string& filename);
    
    std::string m_entities;
    
    std::vector<tBSPFace> m_faces;
    std::vector<int> m_indices;
    std::vector<tBSPVertex> m_verts;
    
    std::vector<tBSPTexture> m_textures;
    std::vector<tBSPLightmap> m_lightmaps;
    
    std::vector<tBSPNode> m_nodes;
    std::vector<tBSPLeaf> m_leafs;
    std::vector<tBSPPlane> m_planes;
    
    std::vector<tBSPBrush> m_brushes;
    std::vector<tBSPBrushSide> m_brushSides;
    std::vector<int> m_leafBrushes;
    
    std::vector<tBSPModel> m_models;
    
    std::vector<tBSPLightVolume> m_lightVolumes;
    
    tBSPVisData m_clusters;
    
    std::filesystem::path m_filepath;
    
    // все пути к текстурам идут относительно этой папки
    // сами уровни лежат в папке maps
    std::filesystem::path m_basepath;
};
