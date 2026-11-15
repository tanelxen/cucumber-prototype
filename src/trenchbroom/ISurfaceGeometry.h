//
//  ISurfaceGeometry.h
//  Cucumber
//
//  Всё, что SceneExtractor и LevelWireframe нужно от статической геометрии уровня.
//  Реализации: MapMesh (запечённая, с лайтмапой) и LevelEditMesh (режим правки, обновляется инкрементально).
//

#pragma once

#include <cstdint>
#include <vector>

#include "Bounds.h"
#include "Mesh.h"
#include "RenderDevice.h"

class ISurfaceGeometry
{
public:
    virtual ~ISurfaceGeometry() = default;

    virtual unsigned int vaoId() const = 0;
    virtual const std::vector<Surface>& surfaces() const = 0;
    virtual const std::vector<Material>& materials() const = 0;
    virtual const std::vector<AABB>& surfaceBounds() const = 0;
    virtual RenderDevice::handle_t lightmapTexture() const = 0;

    // Меняется при каждом изменении геометрии; 0 значит "пусто". Брать из NextGeometryRevision().
    virtual uint64_t getRevision() const = 0;
};
