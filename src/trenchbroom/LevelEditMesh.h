//
//  LevelEditMesh.h
//  Cucumber
//
//  Геометрия уровня в режиме правки на GPU. Тот же интерфейс чтения, что у MapMesh (vaoId, surfaces, materials,
//  surfaceBounds, lightmapTexture, getRevision), поэтому SceneExtractor рисует её теми же поверхностями.
//
//  Отличия от MapMesh:
//    - лайтмапы нет: текстура белая 1x1, вторые UV нулевые;
//    - обновляется инкрементально (см. EditGeometry): правка перегенерирует только изменённые солиды, а в GPU
//      заливаются только затронутые диапазоны (glBufferSubData). Если раскладка чанков изменилась, заливается
//      хвост массивов от первого изменившегося чанка (или всё целиком, если буфер вырос), без повторного
//      приготовления геометрии;
//    - буферы создаются с запасом ёмкости и растут в 1.5 раза, поэтому реаллокации редки.
//  Буферы свои (сырой GL): VertexBuffer / IndexBuffer API обновления не имеют.
//
//  Ревизия берётся из отдельной нумерации со старшим битом, чтобы не совпасть с ревизиями MapMesh.
//

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Bounds.h"
#include "Mesh.h"
#include "RenderDevice.h"
#include "trenchbroom/EditGeometry.h"
#include "level/LevelDocument.h"
#include "level/LevelHistory.h"

class LevelEditMesh
{
public:
    LevelEditMesh();
    ~LevelEditMesh();

    LevelEditMesh(const LevelEditMesh&) = delete;
    LevelEditMesh& operator=(const LevelEditMesh&) = delete;

    // С нуля (вход в режим правки)
    void build(const LevelDocument& level);

    // Применяет изменения документа (из ChangedCallback сессии правки)
    void apply(const LevelDocument& level, const ChangeSet& changes);

    unsigned int vaoId() const { return m_vao; }
    const std::vector<Surface>& surfaces() const { return m_surfaces; }
    const std::vector<Material>& materials() const { return m_materials; }
    const std::vector<AABB>& surfaceBounds() const { return m_surfaceBounds; }
    RenderDevice::handle_t lightmapTexture() const { return m_whiteTexture; }

    // Меняется при каждом изменении меша
    uint64_t getRevision() const { return m_revision; }

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
    struct Buffer
    {
        unsigned int id = 0;
        size_t capacity = 0;   // байт
    };

    uint32_t materialSlot(const std::string& name);
    EditGeometry::SizeResolver sizeResolver();

    void createGpuObjects();
    void uploadWhiteTexture();
    void upload(const EditGeometry::UpdateResult& result);
    // Заливает элементы [first, count) (хвост). Если буфер пришлось увеличить, прежнее содержимое потеряно, и
    // заливается всё с нуля. Возвращает число залитых байт.
    size_t uploadTail(Buffer& buffer, const void* data, size_t count, size_t elementSize, size_t first);
    void uploadRange(const Buffer& buffer, size_t offsetBytes, const void* data, size_t bytes);
    void ensureLightmapBuffer();
    void rebuildSurfaces();

    EditGeometry m_geometry;

    unsigned int m_vao = 0;
    Buffer m_positionBuffer;
    Buffer m_shadingBuffer;
    Buffer m_lightmapBuffer;     // нули: лайтмапы в режиме правки нет, но шейдер ждёт атрибут
    Buffer m_indexBuffer;
    RenderDevice::handle_t m_whiteTexture = 0;

    std::vector<Surface> m_surfaces;
    std::vector<Material> m_materials;
    std::vector<AABB> m_surfaceBounds;
    std::unordered_map<std::string, uint32_t> m_slotByName;

    uint64_t m_revision = 0;
    Stats m_stats;
};
