//
//  Renderer.cpp
//  Cucumber
//

#include "Renderer.h"

#include "RenderDevice.h"
#include "SceneExtractor.h"
#include "ViewBuilder.h"
#include "World.h"

void Renderer::init()
{
    m_shadows.init();
}

void Renderer::shutdown()
{
    m_shadows.shutdown();
}

void Renderer::renderFrame(WorldScene* world, IViewProvider& viewProvider)
{
    m_views.clear();

    if (world)
    {
        viewProvider.collectViews(*world, m_views);

        // Нет видимых видов (все панели свёрнуты) — не тратим время на извлечение сцены и тени
        if (!m_views.empty())
        {
            ExtractRenderScene(*world, m_scene);

            m_shadows.plan(m_scene, m_views);            // CPU: слоты, кэш, какие грани перерисовать
            SubmitViews(m_scene, m_views, m_shadows);    // CPU: свет, отсечение, команды, солнце видов
            m_shadows.renderQueued();                    // GL: тени точечных/прожекторных (с кэшем)
        }
    }

    // Всегда: чистит очереди гизмо/спрайтов/UI, даже когда видов нет
    RenderDevice::flush(m_shadows);
}

const ViewDesc* Renderer::findView(ViewFlagMask flag) const
{
    for (const ViewDesc& view : m_views)
    {
        if (view.flags & flag) return &view;
    }
    return nullptr;
}
