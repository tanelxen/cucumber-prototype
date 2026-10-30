//
//  MapMesh.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 21.10.26.
//
//  Геометрия карты TrenchBroom: один VAO, поверхности = (ячейка сетки x текстура) со своими AABB.
//  Про отсечение и RenderView не знает.
//

#pragma once

#include "MapMeshBuilder.h"
#include "Mesh.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <filesystem>
#include <functional>

class VertexArray;
class VertexBuffer;
class IndexBuffer;

struct LevelDocument;

class MapMesh
{
public:
    MapMesh();
    ~MapMesh();
    
    bool buildFromLevel(const LevelDocument& level, const LightmapUnwrapSettings& unwrapSettings = {});
//    bool buildFromMapAsset(const MapAsset& mapData, const LightmapUnwrapSettings& unwrapSettings = {});
    bool updateLightmapTexture(const std::vector<uint16_t>& halfRgb, std::string& error);
    
    unsigned int vaoId() const;
    const std::vector<Surface>& surfaces() const { return m_surfaces; }
    const std::vector<Material>& materials() const { return m_materials; }
    const std::vector<AABB>& surfaceBounds() const { return m_surfaceBounds; }
    const std::vector<VertexPosition>& positions() const { return m_positions; }
    const std::vector<glm::vec3>& normals() const { return m_normals; }
    const std::vector<uint32_t>& indices() const { return m_indices; }
    const std::vector<glm::vec2>& lightmapTexCoords() const { return m_lightmapTexCoords; }
    int lightmapAtlasSize() const { return m_lightmapAtlasSize; }
    RenderDevice::handle_t lightmapTexture() const { return m_lightmapTexture; }
    
    // 0 = ещё не собрана. Меняется при каждой сборке.
    uint64_t getRevision() const { return m_revision; }
    
private:
    void uploadLightmapTexture(const uint16_t* halfRgb);
    
    using CookFunction = std::function<bool(const MapMeshBuilder::TextureDimensionsResolver&,
                                            MapMeshBuildResult&, std::string&)>;
    bool buildFrom(const std::filesystem::path& basePath, const CookFunction& cook);

    std::unique_ptr<VertexArray> m_vao;
    
    std::unique_ptr<VertexBuffer> m_posBuffer;
    std::unique_ptr<VertexBuffer> m_shadingBuffer;
    std::unique_ptr<VertexBuffer> m_lightmapBuffer;
    std::unique_ptr<IndexBuffer>  m_indexBuffer;
    RenderDevice::handle_t m_lightmapTexture = 0;
    
    std::vector<Surface> m_surfaces;
    std::vector<Material> m_materials;
    std::vector<AABB> m_surfaceBounds;
    std::vector<VertexPosition> m_positions;
    std::vector<glm::vec3> m_normals;
    std::vector<uint32_t> m_indices;
    std::vector<glm::vec2> m_lightmapTexCoords;
    int m_lightmapAtlasSize = 0;
    
    uint64_t m_revision = 0;
};
