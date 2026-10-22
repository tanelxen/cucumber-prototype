//
//  ViewBuilder.cpp
//  Cucumber
//
//  Единственное место, где объекты сцены превращаются в RenderCommand.
//  Mesh и MapMesh больше не знают про RenderView. Кастеры теней сюда больше не попадают: их отбирает ShadowSystem.
//

#include "ViewBuilder.h"

#include "RenderDevice.h"
#include "ShadowSystem.h"
#include "LightComponent.h"   // LightType
#include "Mesh.h"             // Surface, Material

#include <algorithm>

// Свет вида: направленный всегда, остальные — те, чья сфера влияния пересекает фрустум.
// Пока всё влезает в UBO (16), порядок сценовый и стабильный. Если не влезает (например, в редакторе при отлёте
// камеры виден весь уровень), остаются самые важные для ЭТОЙ камеры: intensity * range / (расстояние + range),
// а порядок внутри UBO снова сценовый, чтобы он не "прыгал". Слот тени источнику назначил ShadowSystem::plan.
static void SelectLights(const RenderScene& scene, const ViewDesc& desc, const Frustum& frustum,
                         const ShadowSystem& shadows, std::vector<GPUTimeLight>& out)
{
    struct Candidate { uint32_t index; float score; };
    static std::vector<Candidate> candidates;   // между кадрами сохраняет ёмкость
    candidates.clear();

    constexpr int kDirectional = static_cast<int>(LightType::Directional);

    for (const auto& light : scene.lights)
    {
        if (light.type == kDirectional && out.size() < RenderDevice::MaxLightsPerView) out.push_back(light);   // слот солнца ниже
    }

    for (uint32_t i = 0; i < scene.lights.size(); ++i)
    {
        const GPUTimeLight& light = scene.lights[i];
        if (light.type == kDirectional) continue;

        float score = 0.0f;
        if (light.range > 0.0f)
        {
            AABB influence;
            influence.expand(light.position - glm::vec3(light.range));
            influence.expand(light.position + glm::vec3(light.range));
            if (!frustum.intersects(influence)) continue;

            score = light.intensity * light.range / (glm::distance(desc.camera.position, light.position) + light.range);
        }
        candidates.push_back({ i, score });
    }

    const size_t capacity = RenderDevice::MaxLightsPerView - out.size();
    if (candidates.size() > capacity)
    {
        std::stable_sort(candidates.begin(), candidates.end(),
                         [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
        candidates.resize(capacity);
        std::sort(candidates.begin(), candidates.end(),
                  [](const Candidate& a, const Candidate& b) { return a.index < b.index; });
    }

    for (const Candidate& c : candidates)
    {
        GPUTimeLight copy = scene.lights[c.index];
        copy.shadowSlot = copy.castShadows ? shadows.slotOf(c.index) : -1;
        out.push_back(copy);
    }
}

// Команды для всех поверхностей объекта.
static void EmitObject(RenderView& view, const RenderObject& object)
{
    const GeometryRef& geometry = object.geometry;
    if (geometry.vao == 0 || !geometry.surfaces || !geometry.materials) return;

    // Центр bounds точнее позиции transform; для объектов без bounds — позиция из матрицы
    const glm::vec3 sortPoint = object.bounds.valid() ? object.bounds.center() : glm::vec3(object.world[3]);

    const auto& surfaces = *geometry.surfaces;
    const auto& materials = *geometry.materials;

    for (uint32_t i = 0; i < geometry.surfaceCount; ++i)
    {
        const Surface& surface = surfaces[geometry.firstSurface + i];
        if (surface.materialSlot >= materials.size()) continue;
        const Material& material = materials[surface.materialSlot];

        RenderCommand cmd;
        cmd.vao = geometry.vao;
        cmd.start = surface.startIndex;
        cmd.count = surface.count;

        cmd.pipeline = PipelineType::Generic;
        cmd.featureMask = ShaderFeature::None;
        if (object.bones)            cmd.featureMask |= ShaderFeature::HasSkinning;
        if (material.albedoMap > 0)  cmd.featureMask |= ShaderFeature::HasAlbedoMap;
        if (object.lightMap > 0)     cmd.featureMask |= ShaderFeature::HasLightMap;

        cmd.baseTexture = material.albedoMap;
        cmd.lightMap = object.lightMap;
        cmd.blendMode = material.blendMode;
        cmd.color = material.tintColor;
        cmd.reverseWinding = object.reverseWinding;

        cmd.transform = object.world;
        cmd.bones = object.bones;

        view.submit(cmd, sortPoint);
    }
}

void SubmitViews(const RenderScene& scene, const std::vector<ViewDesc>& views, ShadowSystem& shadows)
{
    // Временный буфер живёт между кадрами (без аллокаций в установившемся режиме)
    static RenderScene::CullResult cull;

    constexpr int kDirectional = static_cast<int>(LightType::Directional);

    for (const ViewDesc& desc : views)
    {
        RenderView& view = RenderDevice::acquireView(desc);

        const Frustum frustum(desc.camera.viewProj());

        SelectLights(scene, desc, frustum, shadows, view.lights);

        // Солнце зависит от камеры вида, поэтому планируется здесь, а не в ShadowSystem::plan
        for (GPUTimeLight& light : view.lights)
        {
            if (light.type != kDirectional || !light.castShadows) continue;

            // BakedAndRealtime-солнце: тень нужна только динамическим приёмникам
            if (light.bakedStatic && scene.dynamicObjects.empty()) break;

            shadows.planSun(scene, desc, light, view.sun);
            if (view.sun.enabled) light.shadowSlot = 0;
            break;
        }

        // Отсечение по камере: одно для всей сцены (карта + меши)
        scene.cull(frustum, desc.layerMask, cull);

        for (uint32_t id : cull.visibleStatic)  EmitObject(view, scene.staticObjects[id]);
        for (uint32_t id : cull.visibleDynamic) EmitObject(view, scene.dynamicObjects[id]);
    }
}
