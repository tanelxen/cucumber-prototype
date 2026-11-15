//
//  LevelEditMesh.h
//  Cucumber
//
//  Геометрия уровня в режиме правки на GPU. Реализует ISurfaceGeometry, поэтому SceneExtractor рисует её
//  теми же поверхностями, что и MapMesh.
//
//  Отличия от MapMesh:
//    - лайтмапы нет: текстура белая 1x1, вторые UV нулевые;
//    - обновляется инкрементально (см. EditGeometry): правка перегенерирует только изменённые солиды, а в GPU
//      заливаются только затронутые диапазоны. Если раскладка чанков изменилась, заливается хвост массивов
//      от первого изменившегося чанка (или всё целиком, если буфер вырос);
//    - буферы создаются без запаса и растут в 1.5 раза (см. GpuBuffer), поэтому реаллокации редки.
//
//  Буферы и VAO те же классы, что у остальных мешей (VertexBuffer / IndexBuffer / VertexArray).
//  Ревизии берутся из общего счётчика NextGeometryRevision().
//

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Bounds.h"
#include "Mesh.h"
#include "RenderDevice.h"
#include "trenchbroom/EditGeometry.h"
#include "trenchbroom/ISurfaceGeometry.h"
#include "trenchbroom/MaterialTable.h"
#include "level/LevelDocument.h"
#include "level/LevelHistory.h"

class VertexArray;
class VertexBuffer;
class IndexBuffer;

class LevelEditMesh final : public ISurfaceGeometry
{
public:
    LevelEditMesh();
    ~LevelEditMesh() override;

    LevelEditMesh(const LevelEditMesh&) = delete;
    LevelEditMesh& operator=(const LevelEditMesh&) = delete;

    // С нуля (вход в режим правки)
    void build(const LevelDocument& level);

    // Применяет изменения документа (из ChangedCallback сессии правки)
    void apply(const LevelDocument& level, const ChangeSet& changes);

    // ISurfaceGeometry
    unsigned int vaoId() const override;
    const std::vector<Surface>& surfaces() const override { return m_surfaces; }
    const std::vector<Material>& materials() const override { return m_materials.materials(); }
    const std::vector<AABB>& surfaceBounds() const override { return m_surfaceBounds; }
    RenderDevice::handle_t lightmapTexture() const override { return m_whiteTexture; }
    uint64_t getRevision() const override { return m_revision; }

    // CPU-копия того, что лежит в GPU (для отладки и тестов)
    const EditGeometry& geometry() const { return m_geometry; }

    struct Stats
    {
        size_t vertices = 0;
        size_t indices = 0;
        size_t surfaces = 0;
        size_t lastUploadBytes = 0;     // сколько байт ушло в GPU последним обновлением
        bool lastLayoutChanged = false; // раскладка чанков менялась (заливался хвост), иначе только диапазоны чанков
    };
    const Stats& stats() const { return m_stats; }

private:
    EditGeometry::SizeResolver sizeResolver();

    void createGpuObjects();
    void uploadWhiteTexture();
    void upload(const EditGeometry::UpdateResult& result);
    void ensureLightmapBuffer();
    void rebuildSurfaces();

    EditGeometry m_geometry;

    std::unique_ptr<VertexArray> m_vao;
    std::unique_ptr<VertexBuffer> m_positionBuffer;
    std::unique_ptr<VertexBuffer> m_shadingBuffer;
    std::unique_ptr<VertexBuffer> m_lightmapBuffer;   // нули: лайтмапы в режиме правки нет, но шейдер ждёт атрибут
    std::unique_ptr<IndexBuffer> m_indexBuffer;
    RenderDevice::handle_t m_whiteTexture = 0;

    std::vector<Surface> m_surfaces;
    std::vector<AABB> m_surfaceBounds;
    MaterialTable m_materials;

    uint64_t m_revision = 0;
    Stats m_stats;
};
