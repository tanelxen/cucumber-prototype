//
//  LightmapController.cpp
//  Cucumber
//

#include "LightmapController.h"

#include "LightmapBakeService.h"
#include "LightmapCache.h"

#include "trenchbroom/MapMesh.h"

#include <utility>

LightmapController::~LightmapController()
{
    cancel();
    *m_alive = false;
}

LightmapBakeSettings LightmapController::makeSettings() const
{
    LightmapBakeSettings settings;
    settings.dilationPasses = m_dilationPasses;
    return settings;
}

void LightmapController::attach(std::shared_ptr<MapMesh> mesh, std::filesystem::path cachePath, int dilationPasses)
{
    cancel();

    m_mesh = std::move(mesh);
    m_cachePath = std::move(cachePath);
    m_dilationPasses = dilationPasses;
    m_bakedLightsHash = 0;
    m_geometryHash = 0;

    if (!m_mesh || m_mesh->lightmapAtlasSize() <= 0) return;

    m_geometryHash = LightmapCache::geometryHash(m_mesh->positions(), m_mesh->lightmapTexCoords(),
                                                 m_mesh->indices(), m_mesh->lightmapAtlasSize());

    LightmapCache::Entry entry;
    std::string error;
    if (!LightmapCache::load(m_cachePath, entry, error))
    {
        m_status = "Lightmap is not baked";
        return;
    }

    // Кэш другой карты или другой UV-раскладки (сменили texelsPerUnit, правили геометрию) — не используем
    if (entry.geometryHash != m_geometryHash || entry.width != m_mesh->lightmapAtlasSize()
        || entry.height != m_mesh->lightmapAtlasSize())
    {
        m_status = "Lightmap cache is for a different map layout; bake again";
        return;
    }

    if (!m_mesh->updateLightmapTexture(entry.pixels, error))
    {
        m_status = "Cannot use lightmap cache: " + error;
        return;
    }

    m_bakedLightsHash = entry.lightsHash;
    m_status = "Lightmap loaded from cache";
}

bool LightmapController::startBake(std::vector<LightmapBakeLight> lights)
{
    if (!m_mesh || m_mesh->lightmapAtlasSize() <= 0)
    {
        m_status = "Cannot bake lighting: map is not loaded";
        return false;
    }
    if (lights.empty())
    {
        m_status = "No enabled baked light sources in the scene";
        return false;
    }

    cancel();   // повторный запуск заменяет предыдущий бейк

    LightmapBakeInput input;
    input.positions = m_mesh->positions();
    input.normals = m_mesh->normals();
    input.indices = m_mesh->indices();
    input.lightmapTexCoords = m_mesh->lightmapTexCoords();
    input.atlasSize = m_mesh->lightmapAtlasSize();
    input.settings = makeSettings();
    input.lights = std::move(lights);

    const uint64_t lightsHash = LightmapCache::lightsHash(input.lights, input.settings);
    const std::weak_ptr<bool> alive = m_alive;
    const std::weak_ptr<MapMesh> weakMesh = m_mesh;

    m_status = "Baking lightmap...";
    m_jobId = LightmapBakeService::Get().submit(std::move(input),
        [this, alive, weakMesh, lightsHash](bool ok, LightmapBakeResult&& result, const std::string& error)
    {
        if (alive.expired()) return;   // контроллера уже нет

        m_jobId = 0;
        const std::shared_ptr<MapMesh> mesh = weakMesh.lock();
        if (!mesh || mesh != m_mesh) return;   // за время бейка карту пересобрали

        if (!ok)
        {
            m_status = "Bake failed: " + error;
            return;
        }

        std::string uploadError;
        if (!mesh->updateLightmapTexture(result.pixels, uploadError))   // главный поток: GL здесь безопасен
        {
            m_status = "Bake failed: " + uploadError;
            return;
        }
        m_bakedLightsHash = lightsHash;

        LightmapCache::Entry entry;
        entry.geometryHash = m_geometryHash;
        entry.lightsHash = lightsHash;
        entry.width = result.width;
        entry.height = result.height;
        entry.pixels = std::move(result.pixels);

        std::string cacheError;
        if (LightmapCache::save(m_cachePath, entry, cacheError))
            m_status = "Lightmap baked successfully";
        else
            m_status = "Lightmap baked, but not saved: " + cacheError;
    });

    return true;
}

void LightmapController::cancel()
{
    if (m_jobId != 0)
    {
        LightmapBakeService::Get().cancel(m_jobId);
        m_jobId = 0;
    }
}

bool LightmapController::isBaking() const
{
    return m_jobId != 0 && LightmapBakeService::Get().isActive(m_jobId);
}

float LightmapController::progress() const
{
    return m_jobId != 0 ? LightmapBakeService::Get().progress(m_jobId) : 0.0f;
}

bool LightmapController::isOutdated(const std::vector<LightmapBakeLight>& currentLights) const
{
    if (m_bakedLightsHash == 0) return !currentLights.empty();   // не запекалась, а запекать есть что
    return LightmapCache::lightsHash(currentLights, makeSettings()) != m_bakedLightsHash;
}
