//
//  GameViewProducer.cpp
//  Cucumber
//

#include "GameViewProducer.h"

#include "CameraManager.h"
#include "CameraComponent.h"
#include "RenderTarget.h"
#include "Entity.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cstddef>

namespace
{
    constexpr float kNear = 0.1f;
    constexpr float kFar = 8192.0f;

    // Матрицы считаем из ИТОГОВОЙ мировой матрицы в момент сбора видов, а не в update():
    // так камера, прикреплённая к движущемуся родителю, не отстаёт на кадр.
    CameraData MakeGameCamera(const glm::mat4& world, float fovDegrees, float aspect)
    {
        CameraData c;
        c.view = glm::inverse(world);
        c.proj = glm::perspective(glm::radians(fovDegrees), aspect, kNear, kFar);
        c.position = glm::vec3(world[3]);
        c.forward = glm::normalize(-glm::vec3(world[2]));
        c.nearZ = kNear;
        c.farZ = kFar;
        return c;
    }
}

bool ProduceGameViews(const CameraManager& cameras, const GameViewParams& params, std::vector<ViewDesc>& out)
{
    const CameraManager::Stack& stack = cameras.stack(params.stackId);
    if (!stack.primary) return false;

    const size_t count = 1 + stack.overlays.size();
    const auto cameraAt = [&](size_t i) { return i == 0 ? stack.primary : stack.overlays[i - 1]; };

    // GameUI рисуется один раз — поверх последней камеры, что пишет в основную цель.
    size_t lastMain = count;
    for (size_t i = 0; i < count; ++i)
    {
        if (!cameraAt(i)->m_renderTarget) lastMain = i;
    }

    int mainOrder = 0;
    int offscreenOrder = 0;

    for (size_t i = 0; i < count; ++i)
    {
        CameraComponent* cam = cameraAt(i);
        const bool offscreen = cam->m_renderTarget != nullptr;

        RenderTarget* target = offscreen ? cam->m_renderTarget : params.target;
        const glm::ivec2 size = offscreen
            ? glm::ivec2(cam->m_renderTarget->width(), cam->m_renderTarget->height())
            : params.size;

        const glm::mat4 world = cam->owner->getTransform().getWorldMatrix();

        ViewDesc v;
        v.id = params.baseViewId + static_cast<uint32_t>(i);
        v.debugName = offscreen ? "GameOffscreen" : (i == 0 ? "Game" : "GameOverlay");
        v.camera = MakeGameCamera(world, cam->fov, float(size.x) / float(size.y));
        v.target = target;
        v.size = size;
        v.layerMask = cam->cullingMask;

        // Оверлей никогда не стирает цвет базовой камеры.
        if (cam->m_isOverlay) v.clear = cam->clearDepthOnly ? ClearMode::Depth : ClearMode::None;
        else                  v.clear = ClearMode::ColorDepth;

        // Камеры, рисующие в текстуру, идут раньше всех, кто может её читать.
        v.order = offscreen ? (-1000 + offscreenOrder++) : mainOrder++;

        v.flags = 0;
        if (params.drawSkybox)                                v.flags |= ViewFlag::Sky;
        if (!offscreen && i == lastMain && params.drawGameUI) v.flags |= ViewFlag::GameUI;
        if (i == 0 && params.audioListener)                   v.flags |= ViewFlag::AudioListener;

        out.push_back(std::move(v));
    }
    return true;
}
