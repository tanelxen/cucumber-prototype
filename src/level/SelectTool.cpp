//
//  SelectTool.cpp
//  Cucumber
//

#include "SelectTool.h"

#include "LevelEditSession.h"
#include "SolidBuilder.h"

namespace
{
    constexpr float kPickPixels = 48.0f;   // насколько далеко от силуэта курсор ещё "рядом" с гранью

    const glm::vec4 kCandidateColor{1.0f, 0.95f, 0.2f, 1.0f};
    const glm::vec4 kPreviewColor{1.0f, 1.0f, 1.0f, 1.0f};
}

void SelectTool::onPointer(LevelEditSession& session, uint32_t viewId, const ViewportPointer& pointer)
{
    if (m_drag.active)
    {
        if (viewId == m_drag.view) updateDrag(session, pointer);
        return;
    }

    m_pushPullMode = pointer.shift && !session.selectedSolids().empty();

    // Кандидата сбрасывает только вид, который его выставил: соседняя панель без курсора его не гасит
    if (!pointer.over || pointer.capturing)
    {
        if (m_candidateView == viewId) m_hasCandidate = false;
        return;
    }

    m_hasCandidate = false;
    m_candidateView = viewId;

    if (m_pushPullMode)
    {
        updateCandidate(session, pointer);
        if (pointer.pressed && m_hasCandidate) beginDrag(session, viewId, pointer);
        return;
    }

    if (!pointer.pressed) return;

    if (!session.hasHover())
    {
        if (!pointer.ctrl) session.clearSelection();
        return;
    }

    const LevelHit& hit = session.hover();
    if (pointer.alt) session.selectFace(hit.solidUid, hit.faceUid);
    else             session.selectSolid(hit.solidUid, pointer.ctrl);
}

void SelectTool::updateCandidate(const LevelEditSession& session, const ViewportPointer& pointer)
{
    const Edit::Ray ray = MakeRay(pointer);
    const Edit::PickTolerance tolerance = PixelTolerance(pointer, kPickPixels);

    double bestDistance = 0.0;
    for (uint64_t uid : session.selectedSolids())
    {
        const Solid* solid = session.findSolid(uid);
        if (!solid) continue;

        FacePick pick;
        if (!PickResizeFace(*solid, ray, tolerance, pick)) continue;
        if (m_hasCandidate && pick.distance >= bestDistance) continue;

        m_hasCandidate = true;
        bestDistance = pick.distance;
        m_candidateSolid = uid;
        m_candidateFace = pick.face;
    }
}

bool SelectTool::beginDrag(const LevelEditSession& session, uint32_t viewId, const ViewportPointer& pointer)
{
    const Solid* solid = session.findSolid(m_candidateSolid);
    if (!solid || m_candidateFace >= solid->faces.size()) return false;

    Drag drag;
    drag.source = *solid;
    drag.planes = SolidPlanes(*solid);
    drag.face = m_candidateFace;
    drag.solidUid = m_candidateSolid;
    drag.normal = drag.planes[drag.face].normal;
    drag.baseDistance = drag.planes[drag.face].distance;
    drag.linePoint = FaceCentroid(*solid, solid->faces[drag.face]);

    // Если луч идёт почти вдоль нормали грани (смотрим на неё в упор), протяжка не определена: не начинаем
    if (!Edit::ClosestLineParam(MakeRay(pointer), drag.linePoint, drag.normal, drag.startParam)) return false;

    drag.preview = drag.source;
    drag.view = viewId;
    drag.active = true;
    m_drag = std::move(drag);
    return true;
}

void SelectTool::updateDrag(LevelEditSession& session, const ViewportPointer& pointer)
{
    if (!pointer.capturing)   // во время полёта камеры курсор скрыт: замираем на последней форме
    {
        double param = 0.0;
        if (Edit::ClosestLineParam(MakeRay(pointer), m_drag.linePoint, m_drag.normal, param))
        {
            const double delta = Edit::Snap(param - m_drag.startParam, session.gridSize());
            if (delta != m_drag.delta)
            {
                std::vector<Geo::Plane> planes = m_drag.planes;
                planes[m_drag.face].distance = m_drag.baseDistance + delta;

                Solid rebuilt;
                std::string error;
                // Недопустимое положение (грань схлопнулась, тело вывернулось) пропускаем: остаёмся на последнем валидном
                if (RebuildSolid(m_drag.source, planes, session.document().materials.size(), rebuilt, error))
                {
                    m_drag.preview = std::move(rebuilt);
                    m_drag.delta = delta;
                }
            }
        }
    }

    if (pointer.released) finishDrag(session);
}

void SelectTool::finishDrag(LevelEditSession& session)
{
    Drag drag = std::move(m_drag);
    m_drag = {};
    m_hasCandidate = false;

    if (drag.delta == 0.0) return;
    session.commit("Resize face", { SolidChange::Modified(std::move(drag.source), std::move(drag.preview)) });
}

std::string SelectTool::hint() const
{
    if (m_drag.active) return "Drag to move the face, release to apply (Esc: cancel)";
    if (m_pushPullMode) return m_hasCandidate ? "Drag the highlighted face" : "Move the cursor near a face of the selected solid";
    return "Click: select  |  Ctrl+click: toggle  |  Alt+click: face  |  Shift: push/pull faces  |  B: box tool";
}

void SelectTool::collectPreview(const LevelEditSession& session, ToolPreview& out) const
{
    if (m_drag.active)
    {
        out.addSolidEdges(m_drag.preview, kPreviewColor);
        out.addFaceOutline(m_drag.preview, m_drag.preview.faces[m_drag.face], kCandidateColor);

        const glm::vec3 center = glm::vec3(FaceCentroid(m_drag.preview, m_drag.preview.faces[m_drag.face]));
        out.addText(center, (m_drag.delta > 0 ? "+" : "") + FormatUnits(m_drag.delta), kCandidateColor);
        return;
    }

    if (!m_hasCandidate) return;

    const Solid* solid = session.findSolid(m_candidateSolid);
    if (!solid || m_candidateFace >= solid->faces.size()) return;

    const Face& face = solid->faces[m_candidateFace];
    out.addFaceOutline(*solid, face, kCandidateColor);

    // Стрелка вдоль нормали показывает, куда грань поедет
    const glm::vec3 center = glm::vec3(FaceCentroid(*solid, face));
    const float length = static_cast<float>(std::max(16.0, session.gridSize() * 2.0));
    out.addLine(center, center + FaceNormal(*solid, face) * length, kCandidateColor);
}
