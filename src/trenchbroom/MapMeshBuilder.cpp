//
//  MapMeshBuilder.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 26.10.26.
//

#include "MapMeshBuilder.h"
#include "MapAsset.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <random>
#include <tuple>
#include <utility>

namespace
{
constexpr float kChunkSize = 1024.0f;
constexpr int kMaxLightmapAtlasSize = 8192;

glm::vec3 Quake2OpenGL(const glm::vec3& in)
{
    return {in.x, in.z, -in.y};
}

bool GetMapPlaneIntersection(const MapPlane& p1, const MapPlane& p2,
                             const MapPlane& p3, glm::vec3& outPoint)
{
    const glm::vec3 cross12 = glm::cross(p1.normal, p2.normal);
    const float denom = glm::dot(cross12, p3.normal);
    if (std::abs(denom) < 1e-4f) return false;

    outPoint = (glm::cross(p2.normal, p3.normal) * p1.distance
              + glm::cross(p3.normal, p1.normal) * p2.distance
              + cross12 * p3.distance) / denom;
    return true;
}

bool IsPointInsideMapBrush(const glm::vec3& point, const MapBrush& brush)
{
    for (const auto& side : brush.sides)
    {
        if (glm::dot(side.plane.normal, point) - side.plane.distance > 0.005f)
            return false;
    }
    return true;
}

glm::vec2 CalculateValve220UV(const glm::vec3& pos, const MapTextureInfo& tex,
                              int texWidth, int texHeight)
{
    const float sx = tex.scaleX != 0.0f ? tex.scaleX : 1.0f;
    const float sy = tex.scaleY != 0.0f ? tex.scaleY : 1.0f;
    const float u = glm::dot(pos, tex.uAxis) / sx + tex.uShift;
    const float v = glm::dot(pos, tex.vAxis) / sy + tex.vShift;
    return {u / static_cast<float>(texWidth), v / static_cast<float>(texHeight)};
}

bool IsNonRenderedTexture(const std::string& name)
{
    return name == "__TB_empty" || name == "skip" || name == "clip";
}

struct BrushFace
{
    size_t sideIndex = 0;
    std::vector<glm::vec3> points;
};

void BuildBrushFaces(const MapBrush& brush, std::vector<BrushFace>& outFaces)
{
    outFaces.clear();
    for (size_t i = 0; i < brush.sides.size(); ++i)
    {
        const auto& faceSide = brush.sides[i];
        std::vector<glm::vec3> facePoints;
        for (size_t j = 0; j < brush.sides.size(); ++j)
        {
            if (i == j) continue;
            for (size_t k = j + 1; k < brush.sides.size(); ++k)
            {
                if (i == k) continue;
                glm::vec3 point;
                if (!GetMapPlaneIntersection(faceSide.plane, brush.sides[j].plane,
                                             brush.sides[k].plane, point)
                    || !IsPointInsideMapBrush(point, brush))
                    continue;

                const bool duplicate = std::find_if(facePoints.begin(), facePoints.end(),
                    [&](const glm::vec3& vertex) {
                        return glm::distance(vertex, point) < 0.005f;
                    }) != facePoints.end();
                if (!duplicate) facePoints.push_back(point);
            }
        }
        if (facePoints.size() < 3) continue;

        glm::vec3 center(0.0f);
        for (const auto& point : facePoints) center += point;
        center /= static_cast<float>(facePoints.size());

        const glm::vec3 normal = faceSide.plane.normal;
        glm::vec3 uAxis = faceSide.texture.uAxis;
        const float uLength = glm::length(uAxis);
        if (uLength < 0.001f || std::abs(glm::dot(uAxis / uLength, normal)) > 0.9f)
        {
            uAxis = std::abs(normal.z) < 0.9f
                ? glm::vec3(0.0f, 0.0f, 1.0f)
                : glm::vec3(1.0f, 0.0f, 0.0f);
        }
        else
        {
            uAxis /= uLength;
        }

        const glm::vec3 vAxis = glm::normalize(glm::cross(normal, uAxis));
        uAxis = glm::cross(vAxis, normal);
        std::vector<std::pair<float, glm::vec3>> sorted;
        sorted.reserve(facePoints.size());
        for (const auto& point : facePoints)
        {
            const glm::vec3 relative = point - center;
            sorted.emplace_back(std::atan2(glm::dot(relative, vAxis),
                                           glm::dot(relative, uAxis)), point);
        }
        std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
            return a.first > b.first;
        });

        BrushFace face;
        face.sideIndex = i;
        face.points.reserve(sorted.size());
        for (const auto& entry : sorted) face.points.push_back(entry.second);
        outFaces.push_back(std::move(face));
    }
}
}

bool MapMeshBuilder::build(const MapAsset& mapData,
                           const LightmapUnwrapSettings& unwrapSettings,
                           const TextureDimensionsResolver& getTextureDimensions,
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
    if (mapData.entities.empty())
    {
        error = "Map has no worldspawn entity";
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
    using ChunkKey = std::tuple<int, int, int, std::string>;
    std::map<ChunkKey, ChunkData> chunks;

    struct FaceData
    {
        ChunkKey chunkKey;
        const MapBrushSide* side = nullptr;
        std::vector<glm::vec3> points;
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

    const auto& worldspawn = mapData.entities[0];
    std::vector<BrushFace> faces;
    for (const auto& brush : worldspawn.brushes)
    {
        if (brush.sides.size() < 4) continue;
        BuildBrushFaces(brush, faces);
        if (faces.empty()) continue;

        AABB brushBounds;
        for (const auto& face : faces)
            for (const auto& point : face.points)
                brushBounds.expand(Quake2OpenGL(point));

        const glm::vec3 cell = brushBounds.center() / kChunkSize;
        const int cx = static_cast<int>(std::floor(cell.x));
        const int cy = static_cast<int>(std::floor(cell.y));
        const int cz = static_cast<int>(std::floor(cell.z));

        for (const auto& face : faces)
        {
            const auto& side = brush.sides[face.sideIndex];
            const std::string& textureName = side.texture.textureName;
            if (IsNonRenderedTexture(textureName)) continue;

            FaceData data;
            data.chunkKey = ChunkKey(cx, cy, cz, textureName);
            data.side = &side;
            data.points = face.points;

            const glm::vec3 normal = glm::normalize(Quake2OpenGL(side.plane.normal));
            const glm::vec3 reference = std::abs(normal.z) < 0.9f
                ? glm::vec3(0.0f, 0.0f, 1.0f)
                : glm::vec3(1.0f, 0.0f, 0.0f);
            const glm::vec3 uAxis = glm::normalize(glm::cross(reference, normal));
            const glm::vec3 vAxis = glm::cross(normal, uAxis);

            data.projectedMin = glm::vec2(std::numeric_limits<float>::max());
            data.projectedMax = glm::vec2(std::numeric_limits<float>::lowest());
            for (const auto& point : face.points)
            {
                const glm::vec3 position = Quake2OpenGL(point);
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

            data.rectWidth = std::max(1, static_cast<int>(std::ceil(texelExtent.x)))
                           + 2 * unwrapSettings.padding;
            data.rectHeight = std::max(1, static_cast<int>(std::ceil(texelExtent.y)))
                            + 2 * unwrapSettings.padding;
            faceData.push_back(std::move(data));
        }
    }

    std::vector<size_t> chartOrder(faceData.size());
    for (size_t i = 0; i < chartOrder.size(); ++i) chartOrder[i] = i;
    std::stable_sort(chartOrder.begin(), chartOrder.end(), [&](size_t a, size_t b) {
        if (faceData[a].rectHeight != faceData[b].rectHeight)
            return faceData[a].rectHeight > faceData[b].rectHeight;
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
            if (chart.rectWidth > atlasSize || chart.rectHeight > atlasSize)
            {
                packed = false;
                break;
            }
            if (shelfX + chart.rectWidth > atlasSize)
            {
                shelfX = 0;
                shelfY += shelfHeight;
                shelfHeight = 0;
            }
            if (shelfY + chart.rectHeight > atlasSize)
            {
                packed = false;
                break;
            }
            chart.atlasX = shelfX;
            chart.atlasY = shelfY;
            shelfX += chart.rectWidth;
            shelfHeight = std::max(shelfHeight, chart.rectHeight);
        }
        if (!packed)
        {
            if (atlasSize >= kMaxLightmapAtlasSize)
            {
                error = "Could not pack lightmap charts into a 32768x32768 atlas";
                return false;
            }
            atlasSize = std::min(atlasSize * 2, kMaxLightmapAtlasSize);
        }
    }

    result.lightmapAtlasSize = atlasSize;
    for (auto& chart : faceData)
    {
        chart.lightmapTexCoords.reserve(chart.projectedPoints.size());
        for (const auto& projected : chart.projectedPoints)
        {
            const glm::vec2 pixel = glm::vec2(chart.atlasX + unwrapSettings.padding,
                                              chart.atlasY + unwrapSettings.padding)
                                  + (projected - chart.projectedMin) * unwrapSettings.texelsPerUnit;
            chart.lightmapTexCoords.push_back(pixel / static_cast<float>(atlasSize));
        }
    }

    std::map<std::string, glm::ivec2> textureDimensions;
    for (const auto& face : faceData)
    {
        const std::string& textureName = std::get<3>(face.chunkKey);
        auto [it, inserted] = textureDimensions.try_emplace(textureName);
        if (inserted)
        {
            it->second = getTextureDimensions ? getTextureDimensions(textureName) : glm::ivec2(256);
            it->second = glm::max(it->second, glm::ivec2(1));
        }

        ChunkData& chunk = chunks[face.chunkKey];
        const uint32_t startIndexOffset = static_cast<uint32_t>(chunk.positions.size());
        for (size_t i = 0; i < face.points.size(); ++i)
        {
            VertexPosition position{Quake2OpenGL(face.points[i])};
            VertexShading shading;
            shading.normal = Quake2OpenGL(face.side->plane.normal);
            shading.texCoord = CalculateValve220UV(face.points[i], face.side->texture,
                                                   it->second.x, it->second.y);
            chunk.positions.push_back(position);
            chunk.shading.push_back(shading);
            chunk.lightmapTexCoords.push_back(face.lightmapTexCoords[i]);
            chunk.bounds.expand(position.position);
        }
        for (size_t t = 1; t + 1 < face.points.size(); ++t)
        {
            chunk.indices.push_back(startIndexOffset);
            chunk.indices.push_back(startIndexOffset + static_cast<uint32_t>(t));
            chunk.indices.push_back(startIndexOffset + static_cast<uint32_t>(t) + 1);
        }
    }

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
        error = "Map has no renderable geometry";
        return false;
    }

    error.clear();
    return true;
}
