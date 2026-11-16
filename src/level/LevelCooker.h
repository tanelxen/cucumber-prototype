//
//  LevelCooker.h
//  Cucumber
//
//  LevelDocument -> данные для рендера (MapMeshBuildResult) и физики. Детерминирован: один и тот же документ
//  всегда даёт один и тот же результат, включая раскладку лайтмапы (от этого зависит кэш лайтмапы).
//

#pragma once

#include "level/LevelDocument.h"

#include "core/Bounds.h"
#include "renderer/Mesh.h"

#include <string>
#include <vector>

#include <glm/glm.hpp>

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

class LevelCooker
{
public:
    using TextureDimensionsResolver = std::function<glm::ivec2(const std::string&)>;
    
    static bool cook(const LevelDocument& level,
                     const LightmapUnwrapSettings& unwrapSettings,
                     const TextureDimensionsResolver& getTextureDimensions,
                     MapMeshBuildResult& result,
                     std::string& error);

    // Вершины каждого солида (пространство движка): из них строятся выпуклые оболочки физики.
    // Включают грани NoDraw: clip-браши обязаны быть твёрдыми.
    static void collisionHulls(const LevelDocument& level, std::vector<std::vector<glm::vec3>>& out);
};
