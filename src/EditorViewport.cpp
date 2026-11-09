//
//  EditorViewport.cpp
//  Cucumber
//

#include "EditorViewport.h"
#include "RenderDevice.h"

void EditorViewport::requestSize(int width, int height, float scale)
{
    if (width < 1 || height < 1) return;

    m_pendingW = width;
    m_pendingH = height;
    m_pendingScale = scale > 0.0f ? scale : 1.0f;
    m_hasPending = true;
}

void EditorViewport::applyPendingSize()
{
    if (!m_hasPending) return;
    m_hasPending = false;

    m_target.resize(m_pendingW, m_pendingH, m_pendingScale); // реаллокация только при реальном изменении
}

void EditorViewport::produceEditorView(std::vector<ViewDesc>& out, bool audioListener, const EditorViewContext& context)
{
    ViewDesc v;
    v.id = m_id * 64;
    v.debugName = "Editor";
    v.camera = m_camera.build(aspect());
    v.target = &m_target;
    v.size = size();
    v.layerMask = 0xFFFFFFFF;
    v.clear = ClearMode::ColorDepth;
    v.order = 0;

    v.flags = ViewFlag::GameUI; // как и раньше: GameUI рисуется и в редакторском виде
    if (drawSkybox)    v.flags |= ViewFlag::Sky;
    if (drawGrid)      v.flags |= ViewFlag::Grid;
    if (drawGizmos)    v.flags |= ViewFlag::Debug;
    if (drawEditOverlay) v.flags |= ViewFlag::EditOverlay;
    if (audioListener) v.flags |= ViewFlag::AudioListener;
    if (drawGizmos || drawEditOverlay) v.flags |= ViewFlag::Debug;
    
    if (shadeMode == ShadeMode::Wireframe)
    {
        // Поверхности карты это слой Static: не берём их в вид, а рёбра рисует набор линий каркаса
        v.layerMask &= ~RenderLayers::Static;
        v.clearColor = glm::vec4(0.11f, 0.11f, 0.12f, 1.0f);
        v.overlayLines = context.wireframeLines;
    }
    
    v.gridStep = context.gridStep;

    out.push_back(std::move(v));
}
