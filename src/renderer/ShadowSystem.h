//
//  ShadowSystem.h
//  Cucumber
//
//  Тени как отдельная подсистема. Заменяет теневой код из RenderDevice.cpp.
//
//  Принципы:
//   1. Тени точечных и прожекторных источников НЕ зависят от камеры. Они считаются один раз за кадр
//      для сцены (а не для каждого вида) и кэшируются по "подписи": если свет и все его кастеры
//      не изменились с прошлого раза, грань кубической карты не перерисовывается.
//   2. Кастеры отбираются по объёму света (граням куба / конусу / ortho-коробке солнца) через
//      RenderScene::queryAABB / cull, а не берутся из команд камеры.
//   3. Источники занимают постоянные слоты (LRU + гистерезис), поэтому кэш переживает кадры.
//   4. Бюджет: за кадр перерисовывается не более maxFaceRendersPerFrame граней/карт.
//      Свет, чья карта ещё ни разу не была готова целиком, временно без тени (слот -1).
//   5. Слот источника передаётся в шейдер через UBO (LightData.parameters.y); цикл по слотам в шейдере не нужен.
//
//  Солнце — единственная тень, зависящая от камеры: считается для каждого базового вида (planSun / renderSun).
//

#pragma once

#include "RenderScene.h"
#include "ShadowTypes.h"
#include "ViewDesc.h"
#include "Shader.h"

#include <array>
#include <cstdint>
#include <vector>

struct ShadowSettings
{
    int pointSlots = 16;               // точечных источников с тенью одновременно
    int pointResolution = 256;         // сторона грани кубической карты (память: slots * 6 * res^2 * 4 байта)
    int spotSlots = 3;                 // 1..3: ограничено размером uLightSpaceMatrices[4] в generic.glsl (слот 0 — солнце)
    int mapResolution = 1024;          // солнце и прожекторы (2D-массив)
    int maxFaceRendersPerFrame = 24;   // бюджет перерисовки за кадр
    uint32_t casterLayers = RenderLayers::Default | RenderLayers::Static;
    float slotHysteresis = 1.3f;       // бонус к приоритету источника, уже владеющего слотом
};

class ShadowSystem
{
public:
    struct Stats
    {
        int shadowedPoint = 0;     // точечных источников с готовой тенью
        int shadowedSpot = 0;
        int facesRendered = 0;     // граней/карт, перерисованных в этом кадре
        int facesCached = 0;       // граней, взятых из кэша
        int facesDeferred = 0;     // не уложились в бюджет
    };

    explicit ShadowSystem(const ShadowSettings& settings = {});
    ~ShadowSystem();
    ShadowSystem(const ShadowSystem&) = delete;
    ShadowSystem& operator=(const ShadowSystem&) = delete;

    void init();       // нужен GL-контекст
    void shutdown();   // освобождает GL-ресурсы; вызвать до уничтожения контекста
    bool ready() const { return m_ready; }

    // --- CPU, один раз за кадр после ExtractRenderScene ---
    void plan(const RenderScene& scene, const std::vector<ViewDesc>& views);

    // Слот тени для источника сцены (по индексу в scene.lights). -1 = без тени.
    // Для точечного — индекс кубической карты, для прожектора — слой в массиве 2D-карт (1..3).
    int slotOf(uint32_t sceneLightIndex) const;

    // Солнце вида. Базовые виды (ClearMode::ColorDepth) получают свою карту, оверлеи используют готовую.
    void planSun(const RenderScene& scene, const ViewDesc& desc, const GPUTimeLight& sun, SunShadow& out);

    // --- GL ---
    void renderQueued();                    // грани точечных/прожекторных источников (после plan, до flush)
    void renderSun(const SunShadow& sun);   // вызывается из RenderDevice::flush для каждого вида
    void bindForSampling() const;           // юниты 2 (2D-массив) и 3 (кубы)

    // uLightSpaceMatrices[4]: [0] — солнце вида, [1..3] — прожекторы
    std::array<glm::mat4, 4> lightSpaceMatrices(const SunShadow& sun) const;

    const Stats& stats() const { return m_stats; }
    const ShadowSettings& settings() const { return m_settings; }

private:
    struct Slot
    {
        uint64_t lightId = 0;       // 0 = свободен
        uint64_t lastUsed = 0;
        std::array<uint64_t, 6> signature{};
        std::array<bool, 6> ready{};
        glm::mat4 matrix{1.0f};     // для прожектора
    };

    struct Candidate { uint32_t index; float score; };
    struct Chosen { uint32_t sceneIndex; int slot; bool point; };

    struct FaceJob
    {
        bool point = true;
        int slot = 0;
        int face = 0;
        glm::mat4 matrix{1.0f};
        glm::vec3 lightPosition{0.0f};
        float range = 0.0f;
        float priority = 0.0f;
        bool slotWasReady = false;
        uint64_t signature = 0;
        std::vector<const RenderObject*> casters;
    };

    float score(const GPUTimeLight& light, const std::vector<ViewDesc>& views) const;
    static int findSlot(const std::vector<Slot>& pool, uint64_t lightId);

    void planPool(const RenderScene& scene, std::vector<Slot>& pool, std::vector<Candidate>& candidates, bool point);
    void planLight(const RenderScene& scene, const GPUTimeLight& light, Slot& slot, int slotIndex, bool point, float score);
    void applyBudget();

    void beginPass();
    void drawCasters(const std::vector<const RenderObject*>& casters, const glm::mat4& viewProjection,
                     bool radialDepth, const glm::vec3& lightPosition, float lightRange);

    ShadowSettings m_settings;
    bool m_ready = false;

    // GL
    Shader m_shader;
    unsigned int m_fbo = 0;
    unsigned int m_pointTexture = 0;   // GL_TEXTURE_CUBE_MAP_ARRAY, pointSlots * 6 слоёв
    unsigned int m_mapTexture = 0;     // GL_TEXTURE_2D_ARRAY, 1 (солнце) + spotSlots слоёв

    // План кадра
    uint64_t m_frame = 0;
    std::vector<Slot> m_pointSlots;
    std::vector<Slot> m_spotSlots;
    std::vector<int> m_lightSlot;
    std::vector<Chosen> m_chosen;
    std::vector<FaceJob> m_jobs;
    Stats m_stats;

    glm::mat4 m_lastSunMatrix{1.0f};
    bool m_haveSun = false;

    // Временные буферы (между кадрами сохраняют ёмкость)
    std::vector<Frustum> m_frustums;
    std::vector<Candidate> m_pointCandidates;
    std::vector<Candidate> m_spotCandidates;
    std::vector<uint8_t> m_claimed;
    std::vector<int> m_assigned;
    std::vector<uint64_t> m_dynamicHash;
    std::vector<const RenderObject*> m_faceCasters;
    RenderScene::CullResult m_cull;
};
