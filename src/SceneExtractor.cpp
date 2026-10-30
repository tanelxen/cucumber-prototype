//
//  SceneExtractor.cpp
//  Cucumber
//

#include "SceneExtractor.h"

#include "World.h"
#include "Entity.h"

#include "MeshComponent.h"
#include "AnimatorComponent.h"
#include "LightComponent.h"
#include "MapComponent.h"

#include "Mesh.h"
#include "trenchbroom/MapMesh.h"

#include <cstdint>
#include <type_traits>

// указатель может быть raw или умным — приводим к raw
template <typename P>
static auto RawPtr(const P& p)
{
    if constexpr (std::is_pointer_v<P>) return p;
    else                                return p.get();
}

// Каждая поверхность карты — отдельный статический объект со своим AABB
static void AddMapSurfaces(const MapMesh& map, RenderScene& out)
{
    const unsigned int vao = map.vaoId();
    if (vao == 0) return;

    const auto& surfaces = map.surfaces();
    const auto& bounds = map.surfaceBounds();

    for (uint32_t i = 0; i < surfaces.size(); ++i)
    {
        RenderObject object;
        object.geometry = { vao, &surfaces, &map.materials(), i, 1 };
        object.bounds = bounds[i];
        object.layer = RenderLayers::Static;
        object.lightMap = map.lightmapTexture();
        out.addStatic(object);
    }
}

void ExtractRenderScene(WorldScene& world, RenderScene& out)
{
    out.beginFrame();

    // --- Статика: карта. Перестраиваем только при смене ревизии (загрузка / reload) ---
    const MapMesh* map = nullptr;
    if (auto* mapComponent = world.getMap())
    {
        map = RawPtr(mapComponent->mesh);
    }

    const uint64_t staticKey = map ? map->getRevision() : 0;
    if (staticKey != out.staticKey())
    {
        out.resetStatic(staticKey);
        if (map) AddMapSurfaces(*map, out);
        out.finalizeStatic();
    }

    // --- Динамика ---
    for (auto& entityPtr : world.m_entities)
    {
        Entity* entity = entityPtr.get();
        if (!entity->m_isActive) continue;   // как и раньше: активность родителей не проверяется

        // --- Меши ---
        if (auto* meshComponent = entity->getComponent<MeshComponent>())
        {
            const Mesh* mesh = RawPtr(meshComponent->mesh);
            const unsigned int vao = mesh ? mesh->getVAOId() : 0;

            if (mesh && vao != 0 && !mesh->m_surfaces.empty())
            {
                RenderObject object;
                object.geometry = { vao, &mesh->m_surfaces, &mesh->m_materials, 0,
                                    static_cast<uint32_t>(mesh->m_surfaces.size()) };
                object.world = entity->getTransform().getWorldMatrix();
                object.layer = meshComponent->layer;

                auto* animator = entity->getComponent<AnimatorComponent>();
                const bool skinned = animator && !animator->m_clips.empty();
                if (skinned)
                {
                    object.bones = &animator->getFinalBoneMatrices();
                }

                // Скиннинг не отсекаем, пока нет bounds анимации (AABB остаётся невалидным)
                const AABB& local = mesh->getBounds();
                if (!skinned && local.valid())
                {
                    object.bounds = local.transformed(object.world);
                }
                else if (skinned && local.valid())
                {
                    // Для камеры скиннинг по-прежнему не отсекается, но тени без границ перерисовывали бы
                    // ВСЕ грани ВСЕХ источников каждый кадр. Берём bounds позы привязки с запасом на анимацию.
                    const glm::vec3 pad = (local.max - local.min) * 0.5f;
                    AABB padded;
                    padded.expand(local.min - pad);
                    padded.expand(local.max + pad);
                    object.shadowBounds = padded.transformed(object.world);
                }

                out.dynamicObjects.push_back(object);
            }
        }

        // --- Свет ---
        if (auto* lightComponent = entity->getComponent<LightComponent>())
        {
            if (!lightComponent->enabled) continue;
            if (lightComponent->mode == LightMode::Baked) continue;

            GPUTimeLight light{};
            light.id = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entity));   // идентичность для кэша теней
            light.type = static_cast<int>(lightComponent->type);
            light.color = lightComponent->color;
            light.intensity = lightComponent->intensity;
            light.range = lightComponent->range;
            light.spotAngle = lightComponent->spotAngle;
            light.castShadows = lightComponent->castShadows;
            light.bakedStatic = (lightComponent->mode == LightMode::BakedAndRealtime);

            auto& transform = entity->getTransform();
            light.position = transform.getWorldPosition();
            light.direction = transform.getWorldRotation() * glm::vec3(0.0f, 0.0f, -1.0f);

            out.lights.push_back(light);
        }
    }
}
