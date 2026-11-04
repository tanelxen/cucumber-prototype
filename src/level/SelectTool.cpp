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
    constexpr float kDragPixels = 4.0f;    // дрожание руки при клике не должно сдвигать солиды

    const glm::vec4 kAxisColors[3] = {
        {1.0f, 0.25f, 0.25f, 1.0f}, {0.35f, 1.0f, 0.35f, 1.0f}, {0.4f, 0.55f, 1.0f, 1.0f}
    };
    const char kAxisNames[3] = { 'X', 'Y', 'Z' };

    // Плоскость движения без ограничений: ортографика это плоскость вида; в перспективе горизонталь, а если смотрим
    // почти горизонтально (горизонталь тогда уезжает в бесконечность), вертикальная плоскость лицом к камере.
    glm::dvec3 MoveFreeNormal(const CameraData& camera)
    {
        const glm::dvec3 forward = glm::normalize(glm::dvec3(camera.forward));
        if (camera.ortho) return forward;
        if (std::abs(forward.y) >= 0.3) return { 0.0, 1.0, 0.0 };
        return glm::normalize(glm::dvec3(forward.x, 0.0, forward.z));
    }

    std::string Signed(double value)
    {
        return (value > 0.0 ? "+" : "") + FormatUnits(value);
    }

    std::string DescribeMove(const glm::dvec3& delta, int axis, bool plane)
    {
        if (axis >= 0 && !plane) return std::string(1, kAxisNames[axis]) + ": " + Signed(delta[axis]);
        if (axis >= 0)
        {
            const int a = std::min((axis + 1) % 3, (axis + 2) % 3);
            const int b = std::max((axis + 1) % 3, (axis + 2) % 3);
            return std::string(1, kAxisNames[a]) + std::string(1, kAxisNames[b]) + ": " + Signed(delta[a]) + ", " + Signed(delta[b]);
        }
        return "X " + Signed(delta.x) + "  Y " + Signed(delta.y) + "  Z " + Signed(delta.z);
    }

    const glm::vec4 kCandidateColor{1.0f, 0.95f, 0.2f, 1.0f};
    const glm::vec4 kPreviewColor{1.0f, 1.0f, 1.0f, 1.0f};
}

void SelectTool::onPointer(LevelEditSession& session, uint32_t viewId, const ViewportPointer& pointer)
{
    // Последний указатель над видом нужен клавишам G и Shift+D
    if (pointer.over && !pointer.capturing)
    {
        m_last = pointer;
        m_lastView = viewId;
        m_hasLast = true;
    }
    else if (m_hasLast && m_lastView == viewId)
    {
        m_hasLast = false;
    }

    if (m_move.active)
    {
        if (viewId == m_move.view) updateMove(session, pointer);
        return;
    }

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

    const LevelHit hit = session.hover();
    if (pointer.alt)  { session.selectFace(hit.solidUid, hit.faceUid, pointer.ctrl); return; }   // Alt: грань, Ctrl+Alt: добавить / убрать
    if (pointer.ctrl) { session.selectSolid(hit.solidUid, true); return; }                      // Ctrl: только переключить выбор

    // Обычное нажатие: выбираем (если солид ещё не выбран) и взводим протяжку. Клик без протяжки остаётся выбором.
    const bool wasSelected = session.isSolidSelected(hit.solidUid);
    if (!wasSelected) session.selectSolid(hit.solidUid, false);

    beginMove(session, viewId, pointer, glm::dvec3(hit.position), true);
    if (m_move.active && wasSelected && session.selectedSolids().size() > 1) m_move.narrowUid = hit.solidUid;
}

bool SelectTool::beginMove(LevelEditSession& session, uint32_t viewId, const ViewportPointer& pointer, const glm::dvec3& grabPoint, bool fromDrag)
{
    const AABB bounds = session.selectionBounds();
    if (!bounds.valid()) return false;

    Move move;
    move.active = true;
    move.fromDrag = fromDrag;
    move.view = viewId;
    move.pressPx = pointer.mousePx;
    move.grabPoint = grabPoint;
    // Плоскость идёт через точку захвата (курсор "держит" её), а к сетке привязывается угол габарита выбранного
    move.constraint.begin(MakeRay(pointer), grabPoint, glm::dvec3(bounds.min), MoveFreeNormal(pointer.camera));

    m_move = std::move(move);
    return true;
}

void SelectTool::updateMove(LevelEditSession& session, const ViewportPointer& pointer)
{
    if (!pointer.capturing)   // во время полёта камеры курсор скрыт: замираем на последнем положении
    {
        glm::dvec3 delta;
        if (m_move.constraint.compute(MakeRay(pointer), session.gridSize(), delta))
        {
            if (m_move.fromDrag && !m_move.moved)
            {
                if (glm::length(pointer.mousePx - m_move.pressPx) < kDragPixels) delta = glm::dvec3(0.0);   // дрожание руки
                else                                                              m_move.moved = true;
            }
            m_move.delta = delta;
        }
    }

    // Протяжка заканчивается отпусканием; кнопка, которая уже не зажата, считается отпущенной (событие могли пропустить,
    // например, отпустили над другим окном): иначе движение зависло бы
    if (m_move.fromDrag ? (pointer.released || !pointer.down) : pointer.pressed) finishMove(session);
}

void SelectTool::finishMove(LevelEditSession& session)
{
    Move move = std::move(m_move);
    m_move = {};

    if (move.delta == glm::dvec3(0.0))
    {
        // Клик без протяжки по выбранному солиду из группы: выбор сужается до него
        if (move.narrowUid != 0 && !move.moved) session.selectSolid(move.narrowUid, false);
        return;
    }

    session.translateSelection(glm::vec3(move.delta), session.textureLock(), move.tag);
}

bool SelectTool::onKey(LevelEditSession& session, LevelKey key, bool shift)
{
    switch (key)
    {
        case LevelKey::X:
        case LevelKey::Y:
        case LevelKey::Z:
            if (!m_move.active) return false;
            m_move.constraint.toggleAxis(static_cast<int>(key) - static_cast<int>(LevelKey::X), shift);   // повтор снимает
            m_move.delta = glm::dvec3(0.0);   // ограничение считается от исходного положения мыши, кадр пересчитает
            return true;

        case LevelKey::G:
        case LevelKey::DuplicateMove:
        {
            if (m_move.active || m_drag.active || session.selectedSolids().empty() || !m_hasLast) return false;

            std::string tag;
            if (key == LevelKey::DuplicateMove)
            {
                // Тег уникален для каждого дублирования: отменённое движение не сольётся с чужой правкой
                tag = "dup.move." + std::to_string(++m_moveSerial);
                if (!session.duplicateSelection(glm::vec3(0.0f), tag)) return false;
            }

            // Захват за центр габарита: плоскость идёт через него, а курсор задаёт, куда он поедет
            const AABB bounds = session.selectionBounds();
            if (!beginMove(session, m_lastView, m_last, glm::dvec3(bounds.center()), false)) return false;
            m_move.tag = tag;
            return true;
        }

        case LevelKey::Enter:
            if (!m_move.active || m_move.fromDrag) return false;
            finishMove(session);
            return true;

        default:
            return false;
    }
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
    if (m_move.active)
    {
        return std::string("X / Y / Z: axis, Shift+axis: plane  |  ") + (m_move.fromDrag ? "release: apply" : "click / Enter: apply") + "  |  Esc: cancel";
    }
    if (m_drag.active) return "Drag to move the face, release to apply (Esc: cancel)";
    if (m_pushPullMode) return m_hasCandidate ? "Drag the highlighted face" : "Move the cursor near a face of the selected solid";
    return "Click: select  |  Drag or G: move  |  Shift+D: duplicate  |  Ctrl+click: toggle  |  Alt+click: face  |  Shift: push/pull  |  B: box  E: vertices";
}

void SelectTool::collectPreview(const LevelEditSession& session, ToolPreview& out) const
{
    if (m_move.active)
    {
        // Каркас сдвинутых солидов; документ до подтверждения не меняется
        for (uint64_t uid : session.selectedSolids())
        {
            if (const Solid* solid = session.findSolid(uid))
                out.addSolidEdges(TranslatedSolid(*solid, glm::vec3(m_move.delta), false), kPreviewColor);
        }

        const int axis = m_move.constraint.axis();
        const glm::vec3 grab(m_move.grabPoint);
        if (axis >= 0)
        {
            for (int a = 0; a < 3; ++a)
            {
                const bool drawn = m_move.constraint.isPlane() ? a != axis : a == axis;
                if (!drawn) continue;

                glm::vec3 direction(0.0f);
                direction[a] = 4096.0f;
                out.addLine(grab - direction, grab + direction, kAxisColors[a]);
            }
        }

        out.addText(grab + glm::vec3(m_move.delta), DescribeMove(m_move.delta, axis, m_move.constraint.isPlane()), kCandidateColor);
        return;
    }

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
