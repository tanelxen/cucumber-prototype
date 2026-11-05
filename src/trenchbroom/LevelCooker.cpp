//
//  LevelCooker.cpp
//  Cucumber
//

#include "LevelCooker.h"

#include "trenchbroom/CookCommon.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <tuple>
#include <string>
#include <utility>

namespace
{
    // Тот же размер ячейки, порядок граней и UV, что у EditGeometry (см. CookCommon.h)
    constexpr int kMaxLightmapAtlasSize = 8192;   // как в прежнем билдере: атлас 32768^2 это гигабайты у бейкера
}

bool LevelCooker::cook(const LevelDocument& level,
                       const LightmapUnwrapSettings& unwrapSettings,
                       const MapMeshBuilder::TextureDimensionsResolver& getTextureDimensions,
                       MapMeshBuildResult& result,
                       std::string& error)
{
    result = {};

    if (!std::isfinite(unwrapSettings.texelsPerUnit) || unwrapSettings.texelsPerUnit <= 0.0f
        || unwrapSettings.padding < 0 || unwrapSettings.padding > kMaxLightmapAtlasSize / 2
        || unwrapSettings.atlasSize <= 0 || unwrapSettings.atlasSize > kMaxLightmapAtlasSize)
    {
        error = "Invalid lightmap unwrap settings";
        return false;
    }
    if (level.solids.empty())
    {
        error = "Level has no solids";
        return false;
    }

    struct ChunkData
    {
        std::vector<VertexPosition> positions;
        std::vector<VertexShading> shading;
        std::vector<glm::vec2> lightmapTexCoords;
        std::vector<uint32_t> indices;
        AABB bounds;
    };
    // Ключ: ячейка (x, y, z) + материал. std::map даёт детерминированный порядок: ячейка, затем материал.
    using ChunkKey = std::tuple<int, int, int, std::string>;
    std::map<ChunkKey, ChunkData> chunks;

    struct FaceData
    {
        ChunkKey chunkKey;
        const Face* face = nullptr;
        glm::vec3 normal{0.0f};
        std::vector<glm::vec3> points;          // в порядке выдачи треугольников
        std::vector<glm::vec2> projectedPoints;
        std::vector<glm::vec2> lightmapTexCoords;
        glm::vec2 projectedMin;
        glm::vec2 projectedMax;
        int atlasX = 0;
        int atlasY = 0;
        int rectWidth = 0;
        int rectHeight = 0;
    };
    std::vector<FaceData> faceData;

    // ---- 1. Грани -> чарты лайтмапы ----
    for (const Solid& solid : level.solids)
    {
        if (solid.faces.empty()) continue;

        const CookCommon::Cell cell = CookCommon::CellOfSolid(solid);
        const int cx = cell.x;
        const int cy = cell.y;
        const int cz = cell.z;

        for (const Face& face : solid.faces)
        {
            if (face.flags & FaceFlag::NoDraw) continue;

            FaceData data;
            data.chunkKey = ChunkKey(cx, cy, cz, level.materials[face.material]);
            data.face = &face;
            data.normal = FaceNormal(solid, face);

            data.points = CookCommon::FacePoints(solid, face);

            // Планарная проекция грани на её плоскость: чарт лайтмапы
            const glm::vec3 reference = std::abs(data.normal.z) < 0.9f ? glm::vec3(0.0f, 0.0f, 1.0f)
                                                                        : glm::vec3(1.0f, 0.0f, 0.0f);
            const glm::vec3 uAxis = glm::normalize(glm::cross(reference, data.normal));
            const glm::vec3 vAxis = glm::cross(data.normal, uAxis);

            data.projectedMin = glm::vec2(std::numeric_limits<float>::max());
            data.projectedMax = glm::vec2(std::numeric_limits<float>::lowest());
            for (const glm::vec3& position : data.points)
            {
                const glm::vec2 projected(glm::dot(position, uAxis), glm::dot(position, vAxis));
                data.projectedPoints.push_back(projected);
                data.projectedMin = glm::min(data.projectedMin, projected);
                data.projectedMax = glm::max(data.projectedMax, projected);
            }

            const glm::vec2 extent = data.projectedMax - data.projectedMin;
            const glm::vec2 texelExtent = extent * unwrapSettings.texelsPerUnit;
            const int maxChartExtent = kMaxLightmapAtlasSize - 2 * unwrapSettings.padding;
            if (!std::isfinite(texelExtent.x) || !std::isfinite(texelExtent.y)
                || texelExtent.x > maxChartExtent || texelExtent.y > maxChartExtent)
            {
                error = "Lightmap chart exceeds the maximum atlas size";
                return false;
            }

            data.rectWidth = std::max(1, static_cast<int>(std::ceil(texelExtent.x))) + 2 * unwrapSettings.padding;
            data.rectHeight = std::max(1, static_cast<int>(std::ceil(texelExtent.y))) + 2 * unwrapSettings.padding;
            faceData.push_back(std::move(data));
        }
    }

    // ---- 2. Упаковка чартов шельфами; атлас растёт, пока всё не поместится ----
    std::vector<size_t> chartOrder(faceData.size());
    for (size_t i = 0; i < chartOrder.size(); ++i) chartOrder[i] = i;
    std::stable_sort(chartOrder.begin(), chartOrder.end(), [&](size_t a, size_t b) {
        if (faceData[a].rectHeight != faceData[b].rectHeight) return faceData[a].rectHeight > faceData[b].rectHeight;
        return faceData[a].rectWidth > faceData[b].rectWidth;
    });

    int atlasSize = unwrapSettings.atlasSize;
    bool packed = false;
    while (!packed)
    {
        int shelfX = 0;
        int shelfY = 0;
        int shelfHeight = 0;
        packed = true;

        for (size_t chartIndex : chartOrder)
        {
            FaceData& chart = faceData[chartIndex];
            if (chart.rectWidth > atlasSize || chart.rectHeight > atlasSize) { packed = false; break; }

            if (shelfX + chart.rectWidth > atlasSize)
            {
                shelfX = 0;
                shelfY += shelfHeight;
                shelfHeight = 0;
            }
            if (shelfY + chart.rectHeight > atlasSize) { packed = false; break; }

            chart.atlasX = shelfX;
            chart.atlasY = shelfY;
            shelfX += chart.rectWidth;
            shelfHeight = std::max(shelfHeight, chart.rectHeight);
        }

        if (!packed)
        {
            if (atlasSize >= kMaxLightmapAtlasSize)
            {
                error = "Could not pack lightmap charts into a " + std::to_string(kMaxLightmapAtlasSize) + "x" + std::to_string(kMaxLightmapAtlasSize) + " atlas";
                return false;
            }
            atlasSize = std::min(atlasSize * 2, kMaxLightmapAtlasSize);
        }
    }

    result.lightmapAtlasSize = atlasSize;
    for (FaceData& chart : faceData)
    {
        chart.lightmapTexCoords.reserve(chart.projectedPoints.size());
        for (const glm::vec2& projected : chart.projectedPoints)
        {
            const glm::vec2 pixel = glm::vec2(chart.atlasX + unwrapSettings.padding, chart.atlasY + unwrapSettings.padding)
                                  + (projected - chart.projectedMin) * unwrapSettings.texelsPerUnit;
            chart.lightmapTexCoords.push_back(pixel / static_cast<float>(atlasSize));
        }
    }

    // ---- 3. Вершины и треугольники по чанкам ----
    std::map<std::string, glm::ivec2> textureDimensions;
    for (const FaceData& data : faceData)
    {
        const std::string& material = std::get<3>(data.chunkKey);
        auto [it, inserted] = textureDimensions.try_emplace(material);
        if (inserted)
        {
            it->second = getTextureDimensions ? getTextureDimensions(material) : glm::ivec2(256);
            it->second = glm::max(it->second, glm::ivec2(1));
        }

        ChunkData& chunk = chunks[data.chunkKey];
        const uint32_t startIndexOffset = static_cast<uint32_t>(chunk.positions.size());

        for (size_t i = 0; i < data.points.size(); ++i)
        {
            VertexPosition position{data.points[i]};

            VertexShading shading;
            shading.normal = data.normal;
            shading.texCoord = CookCommon::ValveUV(data.points[i], data.face->uv, it->second.x, it->second.y);

            chunk.positions.push_back(position);
            chunk.shading.push_back(shading);
            chunk.lightmapTexCoords.push_back(data.lightmapTexCoords[i]);
            chunk.bounds.expand(position.position);
        }

        // Выпуклая грань: веер из первой вершины
        for (size_t t = 1; t + 1 < data.points.size(); ++t)
        {
            chunk.indices.push_back(startIndexOffset);
            chunk.indices.push_back(startIndexOffset + static_cast<uint32_t>(t));
            chunk.indices.push_back(startIndexOffset + static_cast<uint32_t>(t) + 1);
        }
    }

    // ---- 4. Линеаризация: поверхности = (ячейка, материал) ----
    for (const auto& [key, chunk] : chunks)
    {
        if (chunk.indices.empty()) continue;

        MapMeshBuildSurface surface;
        surface.materialName = std::get<3>(key);
        surface.startIndex = static_cast<uint32_t>(result.indices.size());
        surface.count = static_cast<uint32_t>(chunk.indices.size());
        surface.bounds = chunk.bounds;

        const uint32_t vertexOffset = static_cast<uint32_t>(result.positions.size());
        result.positions.insert(result.positions.end(), chunk.positions.begin(), chunk.positions.end());
        result.shading.insert(result.shading.end(), chunk.shading.begin(), chunk.shading.end());
        result.lightmapTexCoords.insert(result.lightmapTexCoords.end(),
                                        chunk.lightmapTexCoords.begin(), chunk.lightmapTexCoords.end());
        for (uint32_t index : chunk.indices) result.indices.push_back(vertexOffset + index);
        result.surfaces.push_back(std::move(surface));
    }

    if (result.positions.empty())
    {
        result = {};
        error = "Level has no renderable geometry";
        return false;
    }

    error.clear();
    return true;
}

void LevelCooker::collisionHulls(const LevelDocument& level, std::vector<std::vector<glm::vec3>>& out)
{
    out.clear();
    out.reserve(level.solids.size());
    for (const Solid& solid : level.solids)
    {
        if (solid.vertices.size() >= 4) out.push_back(solid.vertices);
    }
}
