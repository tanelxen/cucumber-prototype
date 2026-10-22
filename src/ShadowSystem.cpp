//
//  ShadowSystem.cpp
//  Cucumber
//

#include "ShadowSystem.h"

#include "GLState.h"
#include "LightComponent.h"   // LightType
#include "Mesh.h"             // Surface, Material
#include "ShaderVariant.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace
{
    constexpr int kPoint = static_cast<int>(LightType::Point);
    constexpr int kSpot = static_cast<int>(LightType::Spot);
    constexpr int kMaxSpotSlots = 3;   // слой 0 массива 2D-карт занят солнцем

    // --- Хэш подписи (FNV-1a) ---
    constexpr uint64_t kHashSeed = 1469598103934665603ull;

    uint64_t HashBytes(uint64_t h, const void* data, size_t size)
    {
        const auto* bytes = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < size; ++i)
        {
            h ^= bytes[i];
            h *= 1099511628211ull;
        }
        return h;
    }

    template <typename T>
    uint64_t HashValue(uint64_t h, const T& value)
    {
        return HashBytes(h, &value, sizeof(T));
    }

    // Подпись объекта-кастера: геометрия + мировая матрица
    uint64_t HashObject(const RenderObject& object)
    {
        uint64_t h = kHashSeed;
        h = HashValue(h, object.geometry.vao);
        h = HashValue(h, object.geometry.firstSurface);
        h = HashValue(h, object.geometry.surfaceCount);
        h = HashValue(h, object.world);
        return h;
    }

    // --- Матрицы источников ---
    glm::vec3 UpFor(const glm::vec3& direction)
    {
        return std::abs(glm::dot(direction, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.99f
            ? glm::vec3(0.0f, 0.0f, 1.0f)
            : glm::vec3(0.0f, 1.0f, 0.0f);
    }

    glm::mat4 SunMatrix(const glm::vec3& rawDirection, const glm::vec3& cameraPosition)
    {
        const glm::vec3 direction = glm::normalize(rawDirection);
        const glm::mat4 lightView = glm::lookAt(cameraPosition - direction * 4096.0f, cameraPosition, UpFor(direction));
        const glm::mat4 lightProjection = glm::ortho(-2048.0f, 2048.0f, -2048.0f, 2048.0f, 0.1f, 8192.0f);
        return lightProjection * lightView;
    }

    glm::mat4 SpotMatrix(const GPUTimeLight& light)
    {
        const glm::vec3 direction = glm::normalize(light.direction);
        const float angle = glm::clamp(light.spotAngle, 1.0f, 89.0f);
        const glm::mat4 lightView = glm::lookAt(light.position, light.position + direction, UpFor(direction));
        const glm::mat4 lightProjection = glm::perspective(
            glm::radians(angle * 2.0f), 1.0f, 0.1f, glm::max(light.range, 0.2f));
        return lightProjection * lightView;
    }

    std::array<glm::mat4, 6> PointMatrices(const GPUTimeLight& light)
    {
        const glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, glm::max(light.range, 0.2f));
        // Порядок граней куба: +X, -X, +Y, -Y, +Z, -Z (слой = слот * 6 + грань)
        const glm::vec3 directions[] = { {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1} };
        const glm::vec3 ups[] = { {0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0} };

        std::array<glm::mat4, 6> matrices;
        for (size_t i = 0; i < matrices.size(); ++i)
        {
            matrices[i] = projection * glm::lookAt(light.position, light.position + directions[i], ups[i]);
        }
        return matrices;
    }

    AABB SphereBounds(const glm::vec3& center, float radius)
    {
        AABB box;
        box.expand(center - glm::vec3(radius));
        box.expand(center + glm::vec3(radius));
        return box;
    }

    // BakedAndRealtime-источник на лайтмапленной статике не работает (она получает его из лайтмапы), так что
    // рантайм-тень нужна только динамическим приёмникам. Нет таких в радиусе света — слот не тратим.
    bool HasDynamicReceiver(const RenderScene& scene, const GPUTimeLight& light)
    {
        const AABB sphere = SphereBounds(light.position, light.range);
        for (const RenderObject& object : scene.dynamicObjects)
        {
            const AABB& bounds = object.shadowBounds.valid() ? object.shadowBounds : object.bounds;
            if (!bounds.valid() || bounds.intersects(sphere)) return true;
        }
        return false;
    }
}

ShadowSystem::ShadowSystem(const ShadowSettings& settings)
    : m_settings(settings)
{
    m_settings.pointSlots = std::max(0, m_settings.pointSlots);
    m_settings.spotSlots = std::clamp(m_settings.spotSlots, 0, kMaxSpotSlots);
    m_settings.maxFaceRendersPerFrame = std::max(1, m_settings.maxFaceRendersPerFrame);

    m_pointSlots.resize(static_cast<size_t>(m_settings.pointSlots));
    m_spotSlots.resize(static_cast<size_t>(m_settings.spotSlots));
}

ShadowSystem::~ShadowSystem()
{
    shutdown();
}

// =================================================================================================
// GL: ресурсы
// =================================================================================================

void ShadowSystem::init()
{
    if (m_ready) return;

    m_shader.init("resources/shaders/shadow.glsl");

    // --- Кубические карты точечных источников ---
    const int pointLayers = std::max(1, m_settings.pointSlots) * 6;
    glGenTextures(1, &m_pointTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, m_pointTexture);
    glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_DEPTH_COMPONENT24,
                 m_settings.pointResolution, m_settings.pointResolution, pointLayers,
                 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);

    // --- 2D-массив: слой 0 — солнце, 1..N — прожекторы ---
    const int mapLayers = 1 + m_settings.spotSlots;
    glGenTextures(1, &m_mapTexture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_mapTexture);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24,
                 m_settings.mapResolution, m_settings.mapResolution, mapLayers,
                 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
    const GLfloat borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_mapTexture, 0, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    m_ready = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (!m_ready)
    {
        fprintf(stderr, "[ShadowSystem] shadow framebuffer is incomplete\n");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    GLState::invalidate();

    const double pointMB = double(pointLayers) * m_settings.pointResolution * m_settings.pointResolution * 4.0 / (1024.0 * 1024.0);
    const double mapMB = double(mapLayers) * m_settings.mapResolution * m_settings.mapResolution * 4.0 / (1024.0 * 1024.0);
    printf("[ShadowSystem] %d point slots @%d (%.1f MB), %d spot slots + sun @%d (%.1f MB)\n",
           m_settings.pointSlots, m_settings.pointResolution, pointMB,
           m_settings.spotSlots, m_settings.mapResolution, mapMB);
}

void ShadowSystem::shutdown()
{
    if (!m_ready && m_fbo == 0 && m_pointTexture == 0 && m_mapTexture == 0) return;

    if (m_fbo) glDeleteFramebuffers(1, &m_fbo);
    if (m_pointTexture) glDeleteTextures(1, &m_pointTexture);
    if (m_mapTexture) glDeleteTextures(1, &m_mapTexture);
    m_fbo = m_pointTexture = m_mapTexture = 0;
    m_ready = false;
}

// =================================================================================================
// CPU: планирование
// =================================================================================================

int ShadowSystem::findSlot(const std::vector<Slot>& pool, uint64_t lightId)
{
    if (lightId == 0) return -1;
    for (size_t i = 0; i < pool.size(); ++i)
    {
        if (pool[i].lightId == lightId) return static_cast<int>(i);
    }
    return -1;
}

float ShadowSystem::score(const GPUTimeLight& light, const std::vector<ViewDesc>& views) const
{
    const AABB sphere = SphereBounds(light.position, light.range);

    float best = 0.0f;
    for (size_t v = 0; v < views.size(); ++v)
    {
        if (!m_frustums[v].intersects(sphere)) continue;   // свет не влияет на этот вид

        const float distance = glm::distance(views[v].camera.position, light.position);
        best = std::max(best, light.intensity * light.range / (distance + light.range));
    }
    return best;
}

void ShadowSystem::plan(const RenderScene& scene, const std::vector<ViewDesc>& views)
{
    ++m_frame;
    m_stats = Stats{};
    m_jobs.clear();
    m_chosen.clear();
    m_lightSlot.assign(scene.lights.size(), -1);
    m_haveSun = false;

    m_frustums.clear();
    for (const ViewDesc& view : views) m_frustums.emplace_back(view.camera.viewProj());

    m_pointCandidates.clear();
    m_spotCandidates.clear();

    for (uint32_t i = 0; i < scene.lights.size(); ++i)
    {
        const GPUTimeLight& light = scene.lights[i];
        if (!light.castShadows || light.range <= 0.1f || light.id == 0) continue;

        const bool isPoint = light.type == kPoint;
        const bool isSpot = light.type == kSpot && glm::dot(light.direction, light.direction) > 1e-8f;
        if (!isPoint && !isSpot) continue;

        if (light.bakedStatic && !HasDynamicReceiver(scene, light)) continue;

        const float s = score(light, views);
        if (s <= 0.0f) continue;   // не влияет ни на один вид

        (isPoint ? m_pointCandidates : m_spotCandidates).push_back({ i, s });
    }

    if (m_pointCandidates.empty() && m_spotCandidates.empty()) return;

    // Подпись динамического объекта считаем один раз за кадр, а не на каждую грань каждого света
    m_dynamicHash.resize(scene.dynamicObjects.size());
    for (size_t i = 0; i < scene.dynamicObjects.size(); ++i)
    {
        m_dynamicHash[i] = HashObject(scene.dynamicObjects[i]);
    }

    planPool(scene, m_pointSlots, m_pointCandidates, true);
    planPool(scene, m_spotSlots, m_spotCandidates, false);
    applyBudget();

    // Готовность считаем ПОСЛЕ бюджета: свет получает слот для шейдера, когда все его грани отрисованы хотя бы раз
    for (const Chosen& chosen : m_chosen)
    {
        const std::vector<Slot>& pool = chosen.point ? m_pointSlots : m_spotSlots;
        const Slot& slot = pool[static_cast<size_t>(chosen.slot)];
        const int faceCount = chosen.point ? 6 : 1;

        bool allReady = true;
        for (int f = 0; f < faceCount; ++f) allReady = allReady && slot.ready[static_cast<size_t>(f)];

        if (allReady)
        {
            m_lightSlot[chosen.sceneIndex] = chosen.point ? chosen.slot : 1 + chosen.slot;
            (chosen.point ? m_stats.shadowedPoint : m_stats.shadowedSpot)++;
        }
    }
}

void ShadowSystem::planPool(const RenderScene& scene, std::vector<Slot>& pool,
                            std::vector<Candidate>& candidates, bool point)
{
    if (pool.empty() || candidates.empty()) return;

    // Гистерезис: источник, уже владеющий слотом, получает бонус, чтобы слоты не "дрожали" на границе рейтинга
    for (Candidate& c : candidates)
    {
        if (findSlot(pool, scene.lights[c.index].id) >= 0) c.score *= m_settings.slotHysteresis;
    }
    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const Candidate& a, const Candidate& b) { return a.score > b.score; });

    const size_t take = std::min(candidates.size(), pool.size());
    m_claimed.assign(pool.size(), 0);
    m_assigned.assign(take, -1);

    // 1) Те, у кого слот уже есть, его сохраняют (кэш остаётся валидным)
    for (size_t k = 0; k < take; ++k)
    {
        const int s = findSlot(pool, scene.lights[candidates[k].index].id);
        if (s >= 0 && !m_claimed[static_cast<size_t>(s)])
        {
            m_assigned[k] = s;
            m_claimed[static_cast<size_t>(s)] = 1;
        }
    }

    // 2) Новым — свободный слот, иначе вытесняем давно не использованный (LRU)
    for (size_t k = 0; k < take; ++k)
    {
        if (m_assigned[k] >= 0) continue;

        int best = -1;
        for (size_t s = 0; s < pool.size(); ++s)
        {
            if (!m_claimed[s] && pool[s].lightId == 0) { best = static_cast<int>(s); break; }
        }
        if (best < 0)
        {
            for (size_t s = 0; s < pool.size(); ++s)
            {
                if (m_claimed[s]) continue;
                if (best < 0 || pool[s].lastUsed < pool[static_cast<size_t>(best)].lastUsed) best = static_cast<int>(s);
            }
        }

        // best >= 0 гарантирован: take <= pool.size(), а занято не больше k слотов
        Slot& slot = pool[static_cast<size_t>(best)];
        slot = Slot{};
        slot.lightId = scene.lights[candidates[k].index].id;

        m_assigned[k] = best;
        m_claimed[static_cast<size_t>(best)] = 1;
    }

    for (size_t k = 0; k < take; ++k)
    {
        Slot& slot = pool[static_cast<size_t>(m_assigned[k])];
        slot.lastUsed = m_frame;

        m_chosen.push_back({ candidates[k].index, m_assigned[k], point });
        planLight(scene, scene.lights[candidates[k].index], slot, m_assigned[k], point, candidates[k].score);
    }
}

void ShadowSystem::planLight(const RenderScene& scene, const GPUTimeLight& light, Slot& slot,
                             int slotIndex, bool point, float score)
{
    const int faceCount = point ? 6 : 1;

    std::array<glm::mat4, 6> matrices;
    if (point)
    {
        matrices = PointMatrices(light);
    }
    else
    {
        matrices[0] = SpotMatrix(light);
        slot.matrix = matrices[0];
    }

    bool slotWasReady = true;
    for (int f = 0; f < faceCount; ++f) slotWasReady = slotWasReady && slot.ready[static_cast<size_t>(f)];

    // Один запрос по объёму света; дальше каждую грань уточняем её фрустумом
    scene.queryAABB(SphereBounds(light.position, light.range), m_settings.casterLayers, m_cull);

    for (int f = 0; f < faceCount; ++f)
    {
        const Frustum frustum(matrices[static_cast<size_t>(f)]);

        uint64_t signature = kHashSeed;
        signature = HashValue(signature, light.id);
        signature = HashValue(signature, light.position);
        signature = HashValue(signature, light.range);
        if (!point)
        {
            signature = HashValue(signature, light.direction);
            signature = HashValue(signature, light.spotAngle);
        }
        signature = HashValue(signature, f);
        signature = HashValue(signature, scene.staticKey());   // статика = ключ ревизии карты

        m_faceCasters.clear();
        bool alwaysDirty = false;

        for (uint32_t id : m_cull.visibleStatic)
        {
            const RenderObject& object = scene.staticObjects[id];
            if (!object.castShadow || !frustum.intersects(object.bounds)) continue;
            m_faceCasters.push_back(&object);
        }

        for (uint32_t id : m_cull.visibleDynamic)
        {
            const RenderObject& object = scene.dynamicObjects[id];
            if (!object.castShadow) continue;

            const AABB& bounds = object.shadowBounds.valid() ? object.shadowBounds : object.bounds;
            if (bounds.valid() && !frustum.intersects(bounds)) continue;

            m_faceCasters.push_back(&object);
            signature = HashValue(signature, m_dynamicHash[id]);

            // Палитру костей не хэшируем (дорого): скиннинг перерисовывает грань каждый кадр
            if (object.bones) alwaysDirty = true;
        }

        if (!alwaysDirty && slot.ready[static_cast<size_t>(f)] && slot.signature[static_cast<size_t>(f)] == signature)
        {
            ++m_stats.facesCached;
            continue;
        }

        FaceJob job;
        job.point = point;
        job.slot = slotIndex;
        job.face = f;
        job.matrix = matrices[static_cast<size_t>(f)];
        job.lightPosition = light.position;
        job.range = light.range;
        job.priority = score;
        job.slotWasReady = slotWasReady;
        job.signature = signature;
        job.casters = m_faceCasters;
        m_jobs.push_back(std::move(job));
    }
}

void ShadowSystem::applyBudget()
{
    // Сначала то, чего ещё нет вообще (свет без тени заметнее устаревшей тени), затем по приоритету источника
    std::stable_sort(m_jobs.begin(), m_jobs.end(), [](const FaceJob& a, const FaceJob& b) {
        if (a.slotWasReady != b.slotWasReady) return !a.slotWasReady;
        return a.priority > b.priority;
    });

    const size_t budget = static_cast<size_t>(m_settings.maxFaceRendersPerFrame);
    if (m_jobs.size() > budget)
    {
        m_stats.facesDeferred = static_cast<int>(m_jobs.size() - budget);
        m_jobs.resize(budget);
    }

    // Принятые задачи считаем выполненными (renderQueued вызывается сразу за plan)
    for (const FaceJob& job : m_jobs)
    {
        Slot& slot = (job.point ? m_pointSlots : m_spotSlots)[static_cast<size_t>(job.slot)];
        slot.signature[static_cast<size_t>(job.face)] = job.signature;
        slot.ready[static_cast<size_t>(job.face)] = true;
    }
    m_stats.facesRendered = static_cast<int>(m_jobs.size());
}

int ShadowSystem::slotOf(uint32_t sceneLightIndex) const
{
    return sceneLightIndex < m_lightSlot.size() ? m_lightSlot[sceneLightIndex] : -1;
}

void ShadowSystem::planSun(const RenderScene& scene, const ViewDesc& desc, const GPUTimeLight& sun, SunShadow& out)
{
    out.casters.clear();
    out.render = false;
    out.enabled = false;

    if (glm::dot(sun.direction, sun.direction) < 1e-8f) return;

    const bool baseView = desc.clear == ClearMode::ColorDepth;

    if (!baseView)
    {
        // Оверлей (оружие): использует карту последнего базового вида, своей не строит.
        // Раньше оверлей перерисовывал тень только из своих объектов и затирал мировую.
        out.enabled = m_haveSun;
        out.matrix = m_lastSunMatrix;
        return;
    }

    m_lastSunMatrix = SunMatrix(sun.direction, desc.camera.position);
    m_haveSun = true;

    out.enabled = true;
    out.render = true;
    out.matrix = m_lastSunMatrix;

    // Кастеры солнца — всё, что попало в его ortho-коробку, независимо от того, видит ли это камера
    scene.cull(Frustum(out.matrix), desc.layerMask & m_settings.casterLayers, m_cull);

    for (uint32_t id : m_cull.visibleStatic)
    {
        const RenderObject& object = scene.staticObjects[id];
        if (object.castShadow) out.casters.push_back(&object);
    }
    for (uint32_t id : m_cull.visibleDynamic)
    {
        const RenderObject& object = scene.dynamicObjects[id];
        if (object.castShadow) out.casters.push_back(&object);
    }
}

std::array<glm::mat4, 4> ShadowSystem::lightSpaceMatrices(const SunShadow& sun) const
{
    std::array<glm::mat4, 4> matrices;
    matrices.fill(glm::mat4(1.0f));

    if (sun.enabled) matrices[0] = sun.matrix;
    for (size_t i = 0; i < m_spotSlots.size() && i < 3; ++i) matrices[1 + i] = m_spotSlots[i].matrix;

    return matrices;
}

// =================================================================================================
// GL: отрисовка
// =================================================================================================

void ShadowSystem::beginPass()
{
    GLState::bindFramebuffer(m_fbo);
    GLState::depthTest(true);
    GLState::depthFunc(GL_LESS);
    GLState::depthMask(true);
    GLState::cull(false);
    GLState::blend(false);
    GLState::polygonOffset(false);
}

void ShadowSystem::renderQueued()
{
    if (!m_ready || m_jobs.empty()) return;

    GLState::invalidate();
    beginPass();

    for (const FaceJob& job : m_jobs)
    {
        if (job.point)
        {
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_pointTexture, 0, job.slot * 6 + job.face);
            GLState::viewport(0, 0, m_settings.pointResolution, m_settings.pointResolution);
        }
        else
        {
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_mapTexture, 0, 1 + job.slot);
            GLState::viewport(0, 0, m_settings.mapResolution, m_settings.mapResolution);
        }

        glClear(GL_DEPTH_BUFFER_BIT);

        // Точечные пишут радиальную глубину (длина/range). Прожектор и солнце — обычную глубину проекции:
        // именно её читает generic.glsl для shadowType 0 и 2.
        drawCasters(job.casters, job.matrix, job.point, job.lightPosition, job.range);
    }
}

void ShadowSystem::renderSun(const SunShadow& sun)
{
    if (!m_ready || !sun.render) return;

    GLState::invalidate();
    beginPass();

    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_mapTexture, 0, 0);
    GLState::viewport(0, 0, m_settings.mapResolution, m_settings.mapResolution);
    glClear(GL_DEPTH_BUFFER_BIT);

    drawCasters(sun.casters, sun.matrix, false, glm::vec3(0.0f), 0.0f);
}

void ShadowSystem::bindForSampling() const
{
    GLState::bindTextureRaw(2, GL_TEXTURE_2D_ARRAY, m_mapTexture);
    GLState::bindTextureRaw(3, GL_TEXTURE_CUBE_MAP_ARRAY, m_pointTexture);
}

void ShadowSystem::drawCasters(const std::vector<const RenderObject*>& casters, const glm::mat4& viewProjection,
                               bool radialDepth, const glm::vec3& lightPosition, float lightRange)
{
    // Сначала нескиннированные, затем скиннированные: вариант шейдера переключается один раз.
    // Указатель на вариант живёт только до следующего getVariant() (Shader может хранить варианты по-разному).
    for (int pass = 0; pass < 2; ++pass)
    {
        const bool skinnedPass = (pass == 1);

        ShaderVariant* variant = nullptr;
        glm::mat4 lastModel;
        bool modelValid = false;

        for (const RenderObject* object : casters)
        {
            if ((object->bones != nullptr) != skinnedPass) continue;

            const GeometryRef& geometry = object->geometry;
            if (geometry.vao == 0 || !geometry.surfaces || !geometry.materials) continue;

            if (!variant)
            {
                variant = m_shader.getVariant(skinnedPass ? ShaderFeature::HasSkinning : 0);
                if (!variant || !variant->isValid()) break;

                GLState::useProgram(variant->getProgramId());
                variant->setUniform("uVP", viewProjection);
                variant->setUniform("uRadialDepth", radialDepth);
                if (radialDepth)
                {
                    variant->setUniform("uLightPosition", lightPosition);
                    variant->setUniform("uFarPlane", lightRange);
                }
            }

            const ShaderVariant::Locations& loc = variant->locations();

            if (!modelValid || std::memcmp(&lastModel, &object->world, sizeof(glm::mat4)) != 0)
            {
                variant->setUniformAt(loc.model, object->world);
                lastModel = object->world;
                modelValid = true;
            }
            if (skinnedPass)
            {
                variant->setUniformAt(loc.bones, *object->bones);
            }

            GLState::bindVAO(geometry.vao);

            for (uint32_t i = 0; i < geometry.surfaceCount; ++i)
            {
                const Surface& surface = (*geometry.surfaces)[geometry.firstSurface + i];
                if (surface.materialSlot >= geometry.materials->size()) continue;

                // Прозрачные поверхности тень не отбрасывают
                const BlendMode blend = (*geometry.materials)[surface.materialSlot].blendMode;
                if (blend == BlendMode::Transparent || blend == BlendMode::Add) continue;

                const auto bufferOffset = surface.startIndex * sizeof(uint32_t);
                glDrawElements(GL_TRIANGLES, surface.count, GL_UNSIGNED_INT, (void*)bufferOffset);
            }
        }
    }
}
