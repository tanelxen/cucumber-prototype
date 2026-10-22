//
//  MapMeshBuilder.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 26.10.26.
//
//  Сборка геометрии из набора брашей.
//

#pragma once

#include "Bounds.h"
#include "Mesh.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct MapAsset;

struct LightmapUnwrapSettings
{
    float texelsPerUnit = 1.0f / 8.0f;
    int padding = 1;
    int atlasSize = 1024;
};

struct MapMeshBuildSurface
{
    std::string materialName;
    uint32_t startIndex = 0;
    uint32_t count = 0;
    AABB bounds;
};

struct MapMeshBuildResult
{
    std::vector<VertexPosition> positions;
    std::vector<VertexShading> shading;
    std::vector<glm::vec2> lightmapTexCoords;
    std::vector<uint32_t> indices;
    std::vector<MapMeshBuildSurface> surfaces;
    int lightmapAtlasSize = 0;
};

class MapMeshBuilder
{
public:
    using TextureDimensionsResolver = std::function<glm::ivec2(const std::string&)>;

    static bool build(const MapAsset& mapData,
                      const LightmapUnwrapSettings& unwrapSettings,
                      const TextureDimensionsResolver& getTextureDimensions,
                      MapMeshBuildResult& result,
                      std::string& error);
};
