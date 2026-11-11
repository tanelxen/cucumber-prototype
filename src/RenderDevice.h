//
//  RenderDevice.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 19.03.26.
//
//  RenderView = ViewDesc (откуда и куда рисуем) + команды, собранные для этого вида.
//  RenderViewRequest и ViewAdapter удалены: описание вида живёт в одном месте — ViewDesc.
//

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>   // TODO: убрать из публичного заголовка, когда handle_t перестанет тянуть GL

#include "ViewDesc.h"
#include "ShadowTypes.h"

class Camera;
class RenderTarget;
struct RenderView;
struct VertexLayout;
class ShadowSystem;

namespace RenderDevice {
    
    typedef unsigned int handle_t;
    constexpr std::size_t MaxLightsPerView = 16;
    
    void init();
    
    handle_t makeTexture(int width, int height, bool hasAlpha, bool srgb, const void* data);
    void deleteTexture(handle_t handle);
    
    handle_t makeVertexBuffer(long size, void* data);
    handle_t makeIndexBuffer(long size, void* data);
    
    handle_t getWhiteTexture();
    
    inline std::vector<glm::vec3> shCoefficients(9, glm::vec3(0.0f));
    
    // Выдаёт пустой RenderView для указанного вида из пула (векторы сохраняют ёмкость между кадрами).
    // Ссылка действительна до следующего acquireView() и до flush().
    RenderView& acquireView(const ViewDesc& desc);
    
    // Сортирует виды и рисует их (для каждого — солнечная тень + сцена), чистит накопленные списки.
    // Точечные/прожекторные карты теней к этому моменту уже отрисованы: ShadowSystem::renderQueued().
    void flush(ShadowSystem& shadows);
}

enum class BlendMode
{
    Opaque, Mask, Add, Transparent
};

enum class PipelineType
{
    Generic, Grid, FlatShaded
};

using FeatureMask = uint32_t;

namespace ShaderFeature
{
    constexpr FeatureMask None            = 0;
    constexpr FeatureMask HasAlbedoMap    = 1 << 0;
    constexpr FeatureMask HasNormalMap    = 1 << 1;
    constexpr FeatureMask HasSkinning     = 1 << 2;
    constexpr FeatureMask HasShadows      = 1 << 3;
    constexpr FeatureMask HasSpherical    = 1 << 4;
    constexpr FeatureMask HasLightMap     = 1 << 5;
}

namespace RenderLayers
{
    const uint32_t Default    = 1 << 0; // 1  (Обычные объекты мира)
    const uint32_t Static     = 1 << 1; // 2  (BSP геометрия, стены)
    const uint32_t Weapon     = 1 << 2; // 4  (Оружие в руках игрока)
    const uint32_t Transparent= 1 << 3; // 8  (Стекла, частицы)
    const uint32_t UI         = 1 << 4; // 16 (Внутриигровой интерфейс)
    
    const uint32_t Everything = 0xFFFFFFFF;
}

struct RenderCommand
{
    // Mesh
    RenderDevice::handle_t vao = 0;
    RenderDevice::handle_t ibo = 0;
    unsigned int start = 0;
    unsigned int count = 0;
    
    // Material
    PipelineType pipeline = PipelineType::Generic;
    FeatureMask featureMask = 0;
    RenderDevice::handle_t baseTexture = 0;
    RenderDevice::handle_t lightMap = 0;
    BlendMode blendMode = BlendMode::Opaque;
    bool reverseWinding = false;
    glm::vec4 color = {1, 1, 1, 1};
    
    glm::mat4 transform = glm::mat4(1);
    
    // Animation
    const std::vector<glm::mat4>* bones = nullptr;
    
    // Заполняется в RenderView::submit(). Руками не трогать.
    uint64_t sortKey = 0;
};

struct GPUTimeLight
{
    glm::vec3 position;  // Мировые координаты
    glm::vec3 direction; // Направление (для солнца)
    glm::vec3 color;
    float intensity;
    float range;
    float spotAngle;
    bool castShadows;
    int type;            // LightType value
    
    // Заполняются не рендером сцены, а ShadowSystem / ViewBuilder:
    uint64_t id = 0;     // стабильная идентичность источника между кадрами (для кэша теней)
    int shadowSlot = -1; // индекс карты тени для шейдера (-1 = без тени)
    
    // LightMode::BakedAndRealtime. На лайтмапленных (статических) поверхностях источник уже запечён,
    // поэтому шейдер пропускает его там; рантайм-вклад получают только динамические объекты.
    bool bakedStatic = false;
};

// Ключи сортировки. Старшие биты — то, что дороже всего переключать.
namespace RenderSort
{
    // Для неотрицательных float порядок битовых представлений совпадает с порядком значений.
    inline uint32_t FloatBits(float f)
    {
        uint32_t bits;
        std::memcpy(&bits, &f, sizeof(bits));
        return bits;
    }
    
    // [pipeline:2][blend:2][features:8][texture:20][vao:16][distance:16] — спереди назад
    inline uint64_t OpaqueKey(const RenderCommand& c, float distanceSq)
    {
        return (uint64_t(static_cast<uint32_t>(c.pipeline) & 0x3u) << 62) |
               (uint64_t(static_cast<uint32_t>(c.blendMode) & 0x3u) << 60) |
               (uint64_t(c.featureMask & 0xFFu) << 52) |
               (uint64_t(c.baseTexture & 0xFFFFFu) << 32) |
               (uint64_t(c.vao & 0xFFFFu) << 16) |
               uint64_t(FloatBits(distanceSq) >> 16);
    }
    
    // [~distance:32][blend:2][texture:12][vao:16] — сзади вперёд; остальное только для стабильности порядка
    inline uint64_t TransparentKey(const RenderCommand& c, float distanceSq)
    {
        return (uint64_t(~FloatBits(distanceSq)) << 32) |
               (uint64_t(static_cast<uint32_t>(c.blendMode) & 0x3u) << 28) |
               (uint64_t(c.baseTexture & 0xFFFu) << 16) |
               uint64_t(c.vao & 0xFFFFu);
    }
}

// Всё, что нужно для отрисовки одного вида: его описание и собранные команды.
struct RenderView
{
    ViewDesc desc;
    
    std::vector<GPUTimeLight> lights;
    
    std::vector<RenderCommand> opaquePass;
    std::vector<RenderCommand> transparentPass;
    
    // Солнечная тень этого вида (зависит от камеры). Точечные и прожекторные тени от вида не зависят.
    SunShadow sun;
    
    // Возвращает вид в исходное состояние для нового описания, сохраняя ёмкость векторов.
    void reset(const ViewDesc& newDesc)
    {
        desc = newDesc;
        lights.clear();
        opaquePass.clear();
        transparentPass.clear();
        sun.enabled = false;
        sun.render = false;
        sun.casters.clear();
    }
    
    // sortPoint — мировая точка для сортировки по глубине (центр объекта/поверхности).
    // Параметр обязательный: забыть его нельзя.
    void submit(const RenderCommand& cmd, const glm::vec3& sortPoint)
    {
        const bool translucent = cmd.blendMode == BlendMode::Transparent || cmd.blendMode == BlendMode::Add;
        
        const glm::vec3 delta = sortPoint - desc.camera.position;
        const float distanceSq = glm::dot(delta, delta);
        
        RenderCommand& stored = translucent ? transparentPass.emplace_back(cmd) : opaquePass.emplace_back(cmd);
        stored.sortKey = translucent ? RenderSort::TransparentKey(stored, distanceSq)
                                     : RenderSort::OpaqueKey(stored, distanceSq);
    }
    
    void sortCommands()
    {
        const auto byKey = [](const RenderCommand& a, const RenderCommand& b) { return a.sortKey < b.sortKey; };
        std::stable_sort(opaquePass.begin(), opaquePass.end(), byKey);
        std::stable_sort(transparentPass.begin(), transparentPass.end(), byKey);
    }
};
