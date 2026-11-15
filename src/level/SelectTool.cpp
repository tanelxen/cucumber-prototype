//
//  SelectTool.cpp
//  Cucumber
//

#include "SelectTool.h"

#include "LevelEditSession.h"
#include "SolidBuilder.h"

namespace
{
    constexpr float kEdgeHandlePixels = 8.0f;   // ортографика: насколько близко к ребру курсор внутри силуэта берёт грань
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
    const glm::vec4 kAltFaceColor{0.3f, 1.0f, 1.0f, 1.0f};
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

    // Alt: выбор грани. При выбранном солиде (или гранях) работаем только с гранями этого солида, иначе с уровнем
    const std::vector<uint64_t> context = session.contextSolids();
    m_altMode = !m_pushPullMode && pointer.alt;

    // Кандидата сбрасывает только вид, который его выставил: соседняя панель без курсора его не гасит
    if (!pointer.over || pointer.capturing)
    {
        if (m_candidateView == viewId) { m_hasCandidate = false; m_altValid = false; }
        return;
    }

    m_hasCandidate = false;
    m_altValid = false;
    m_candidateView = viewId;

    if (m_pushPullMode)
    {
        updateCandidate(session, pointer);
        if (pointer.pressed && m_hasCandidate) beginDrag(session, viewId, pointer);
        return;
    }

    if (m_altMode)
    {
        updateAltFace(session, pointer, context);
        if (pointer.pressed && m_altValid) session.selectFace(m_altSolid, m_altFaceUid, pointer.ctrl);   // Ctrl+Alt добавляет / убирает
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

    // Обычное нажатие по невыбранному солиду только выбирает его: случайно сдвинуть солид, просто наведя и потянув ЛКМ,
    // нельзя (особенно в ортографике, где в луч попадает всё подряд). Тащить можно только солид, выбранный заранее.
    // Солид под курсором это первый, в который попал луч: если выбранный закрыт другим, нажатие выберет закрывающий
    // (как в TrenchBroom), а выбранный двигается клавишей G.
    if (!session.isSolidSelected(hit.solidUid))
    {
        session.selectSolid(hit.solidUid, false);
        return;
    }

    // Нажатие по выбранному взводит протяжку. Клик без протяжки остаётся кликом (в группе сужает выбор).
    beginMove(session, viewId, pointer, glm::dvec3(hit.position), true);
    if (m_move.active && session.selectedSolids().size() > 1) m_move.narrowUid = hit.solidUid;
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
    // Только выбранные солиды: остальной уровень луч не заслоняет
    std::vector<const Solid*> solids;
    std::vector<uint64_t> uids;
    for (uint64_t uid : session.selectedSolids())
    {
        if (const Solid* solid = session.findSolid(uid)) { solids.push_back(solid); uids.push_back(uid); }
    }
    if (solids.empty()) return;

    // Ортографика: внутри силуэта грань берётся только у самого ребра, радиус ручки в мировых единицах
    const double radius = pointer.camera.ortho ? PixelTolerance(pointer, kEdgeHandlePixels).base : 0.0;

    ExtrudePick pick;
    if (!PickExtrudeFace(solids, MakeRay(pointer), pointer.camera.ortho, radius, pick)) return;

    m_hasCandidate = true;
    m_candidatePick = pick;
    m_candidateSolid = uids[pick.solidIndex];
    m_candidateFace = pick.face;
}

void SelectTool::updateAltFace(const LevelEditSession& session, const ViewportPointer& pointer, const std::vector<uint64_t>& context)
{
    const Edit::Ray ray = MakeRay(pointer);

    // Сначала грани контекстных солидов (за другими солидами тоже), и только промахнувшись берём то, что под курсором в уровне
    double best = 0.0;
    for (uint64_t uid : context)
    {
        const Solid* solid = session.findSolid(uid);
        if (!solid) continue;

        float distance = 0.0f;
        size_t face = 0;
        if (!RaycastSolid(*solid, glm::vec3(ray.origin), glm::vec3(ray.dir), static_cast<float>(ray.maxDistance), distance, face)) continue;
        if (m_altValid && distance >= best) continue;

        m_altValid = true;
        best = distance;
        m_altSolid = uid;
        m_altFaceUid = solid->faces[face].uid;
    }

    if (!m_altValid && session.hasHover())
    {
        m_altValid = true;
        m_altSolid = session.hover().solidUid;
        m_altFaceUid = session.hover().faceUid;
    }
}

bool SelectTool::DragParam(const Drag& drag, const Edit::Ray& ray, double& out)
{
    if (!drag.usePlane) return Edit::ClosestLineParam(ray, drag.referencePoint, drag.normal, out);

    double t = 0.0;
    if (!Edit::RayPlane(ray, drag.referencePoint, drag.referenceNormal, t)) return false;
    out = glm::dot(ray.at(t) - drag.referencePoint, drag.normal);   // курсор в опорной плоскости, смещение это проекция на нормаль
    return true;
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
    drag.usePlane = m_candidatePick.kind == ExtrudePick::Kind::Horizon;
    drag.referencePoint = m_candidatePick.handlePoint;
    drag.referenceNormal = m_candidatePick.referenceNormal;

    // Если курсор не определяет смещение (луч параллелен прямой или плоскости), протяжка не начинается
    if (!DragParam(drag, MakeRay(pointer), drag.startParam)) return false;

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
        if (DragParam(m_drag, MakeRay(pointer), param))
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
    if (m_altMode) return "Alt+click: select a face of the selected solid (even behind other solids), Ctrl+Alt+click: add / remove";
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

    if (m_altMode && m_altValid)
    {
        if (const Solid* solid = session.findSolid(m_altSolid))
        {
            for (const Face& face : solid->faces)
            {
                if (face.uid == m_altFaceUid) out.addFaceOutline(*solid, face, kAltFaceColor);
            }
        }
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
