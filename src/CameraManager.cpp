//
//  CameraManager.cpp
//  Cucumber
//

#include "CameraManager.h"

#include "CameraComponent.h"
#include "Entity.h"

#include <algorithm>

static bool IsLive(const CameraComponent* camera)
{
    return camera->enabled && camera->owner && camera->owner->m_isActive;
}

void CameraManager::add(CameraComponent* camera)
{
    if (std::find(m_cameras.begin(), m_cameras.end(), camera) != m_cameras.end()) return;
    m_cameras.push_back(camera);
    rebuild();
}

void CameraManager::remove(CameraComponent* camera)
{
    m_cameras.erase(std::remove(m_cameras.begin(), m_cameras.end(), camera), m_cameras.end());
    rebuild();
}

const CameraManager::Stack& CameraManager::stack(uint32_t stackId) const
{
    static const Stack kEmpty;
    const auto it = m_stacks.find(stackId);
    return it != m_stacks.end() ? it->second : kEmpty;
}

void CameraManager::rebuild()
{
    // Сохраняем записи (и ёмкость векторов), только обнуляем содержимое.
    for (auto& [id, s] : m_stacks)
    {
        s.primary = nullptr;
        s.overlays.clear();
    }
    for (auto* camera : m_cameras)
    {
        m_stacks.try_emplace(camera->cameraStackId);
    }

    for (auto& [id, s] : m_stacks)
    {
        CameraComponent* explicitPrimary = nullptr;
        CameraComponent* firstBase = nullptr;

        for (auto* camera : m_cameras)
        {
            if (camera->cameraStackId != id || !IsLive(camera)) continue;

            if (camera->m_isOverlay)
            {
                s.overlays.push_back(camera);
                continue;
            }

            if (!firstBase) firstBase = camera;
            if (!explicitPrimary && camera->m_isPrimary) explicitPrimary = camera;
        }

        s.primary = explicitPrimary ? explicitPrimary : firstBase;
        std::stable_sort(s.overlays.begin(), s.overlays.end(),
                         [](const CameraComponent* a, const CameraComponent* b) { return a->depth < b->depth; });
    }

    // m_isPrimary сериализуется, поэтому держим флаг согласованным. Это единственное место записи.
    for (auto* camera : m_cameras)
    {
        camera->m_isPrimary = (stack(camera->cameraStackId).primary == camera);
    }
}

bool CameraManager::setPrimary(CameraComponent* camera)
{
    if (!camera || !camera->enabled || camera->m_isOverlay) return false;
    if (std::find(m_cameras.begin(), m_cameras.end(), camera) == m_cameras.end()) return false;

    for (auto* other : m_cameras)
    {
        if (other->cameraStackId == camera->cameraStackId) other->m_isPrimary = false;
    }
    camera->m_isPrimary = true;

    rebuild();
    return true;
}
