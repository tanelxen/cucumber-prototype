//
//  Renderer.h
//  Cucumber
//
//  Фронтенд рендера: один вызов за кадр превращает "мир + список видов" в картинку.
//  Владеет RenderScene, списком видов и ShadowSystem.
//
//  Кадр:   collectViews -> Extract -> shadows.plan -> SubmitViews -> shadows.renderQueued -> RenderDevice::flush
//

#pragma once

#include "IViewProvider.h"
#include "RenderScene.h"
#include "ShadowSystem.h"
#include "ViewDesc.h"

#include <vector>

class WorldScene;

class Renderer
{
public:
    explicit Renderer(const ShadowSettings& shadowSettings = {}) : m_shadows(shadowSettings) {}

    // После RenderDevice::init() (нужен GL-контекст).
    void init();

    // До уничтожения GL-контекста.
    void shutdown();

    // world может быть nullptr (нет активной сцены): тогда только чистятся очереди рендера.
    void renderFrame(WorldScene* world, IViewProvider& viewProvider);

    // Виды последнего кадра: нужны тем, кому важна камера (например, аудио-слушателю).
    const std::vector<ViewDesc>& views() const { return m_views; }
    const ViewDesc* findView(ViewFlagMask flag) const;

    ShadowSystem& shadows() { return m_shadows; }
    const RenderScene& scene() const { return m_scene; }

private:
    RenderScene m_scene;
    std::vector<ViewDesc> m_views;
    ShadowSystem m_shadows;
};
