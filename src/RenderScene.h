//
//  RenderScene.h
//  Cucumber
//
//  Снимок мира для рендера и ЕДИНАЯ точка отсечения для всей сцены.
//
//  Два набора объектов:
//   - staticObjects: меняются редко (поверхности карты). Лежат в SpatialGrid и перестраиваются только
//     при смене staticKey (новая карта / перезагрузка).
//   - dynamicObjects: меши-инстансы. Заполняются каждый кадр и проверяются по фрустуму линейно:
//     для сотен-тысяч объектов это дешевле, чем перестраивать сетку каждый кадр.
//     Когда динамических объектов станет заметно больше (порядка тысяч) — переведём их на вторую сетку.
//
//  Вызывающий код про разделение не знает: он вызывает cull() / queryAABB() и получает id.
//

#pragma once

#include "Bounds.h"
#include "SpatialGrid.h"
#include "RenderDevice.h"   // GPUTimeLight, RenderLayers

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

struct Surface;   // Mesh.h
struct Material;  // Mesh.h

// "Диапазон поверхностей в таблице поверхностей". Одинаково описывает и меш целиком, и одну поверхность карты;
// поэтому построение команд не зависит от того, откуда геометрия.
struct GeometryRef
{
    unsigned int vao = 0;
    const std::vector<Surface>* surfaces = nullptr;
    const std::vector<Material>* materials = nullptr;
    uint32_t firstSurface = 0;
    uint32_t surfaceCount = 0;
};

struct RenderObject
{
    GeometryRef geometry;

    glm::mat4 world{1.0f};
    AABB bounds;                    // мировые границы; !valid() => объект не отсекается (например, скиннинг)
    AABB shadowBounds;              // консервативные границы для теней, когда bounds невалиден (скиннинг); иначе не нужен

    uint32_t layer = RenderLayers::Default;
    const std::vector<glm::mat4>* bones = nullptr;   // палитра костей; живёт до следующего update аниматора
    RenderDevice::handle_t lightMap = 0;

    bool reverseWinding = false;    // карта из TrenchBroom
    bool castShadow = true;         // TODO: брать из MeshComponent, когда там появится флаг
};

class RenderScene
{
public:
    // Результат отсечения. Индексы в staticObjects / dynamicObjects соответственно.
    struct CullResult
    {
        std::vector<uint32_t> visibleStatic;
        std::vector<uint32_t> visibleDynamic;
    };

    std::vector<RenderObject> staticObjects;
    std::vector<RenderObject> dynamicObjects;
    std::vector<GPUTimeLight> lights;

    // --- Заполнение (SceneExtractor) ---
    void beginFrame();                       // чистит динамику и свет, статику не трогает

    uint64_t staticKey() const { return m_staticKey; }
    void resetStatic(uint64_t key);          // очистить статику и запомнить идентификатор её содержимого
    void addStatic(const RenderObject& object);
    void finalizeStatic();                   // построить сетку

    // --- Запросы ---
    // Объекты, пересекающие фрустум и проходящие по маске слоёв.
    void cull(const Frustum& frustum, uint32_t layerMask, CullResult& out) const;

    // Объекты, пересекающие объём (сфера/конус/ortho-коробка света -> AABB). Для теней (этап 4).
    void queryAABB(const AABB& box, uint32_t layerMask, CullResult& out) const;

private:
    SpatialGrid m_staticGrid{1024.0f};
    mutable SpatialGrid::QueryScratch m_scratch;   // запросы однопоточные
    uint64_t m_staticKey = 0;
};
