//
//  VertexTool.cpp
//  Cucumber
//

#include "VertexTool.h"

#include "LevelEditSession.h"
#include "SolidBuilder.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace
{
    constexpr float kVertexPickPixels = 10.0f;
    constexpr float kEdgePickPixels = 8.0f;
    constexpr float kDragPixels = 4.0f;
    constexpr float kSamePosition = 0.02f;
    constexpr double kGuideLength = 4096.0;

    const glm::vec4 kVertexColor{0.85f, 0.85f, 0.85f, 1.0f};
    const glm::vec4 kHoverColor{1.0f, 0.95f, 0.2f, 1.0f};
    const glm::vec4 kSelectedColor{1.0f, 0.45f, 0.1f, 1.0f};
    const glm::vec4 kPreviewColor{1.0f, 1.0f, 1.0f, 1.0f};
    const glm::vec4 kAxisColors[3] = {
        {1.0f, 0.25f, 0.25f, 1.0f}, {0.35f, 1.0f, 0.35f, 1.0f}, {0.4f, 0.55f, 1.0f, 1.0f}
    };
    const char kAxisNames[3] = { 'X', 'Y', 'Z' };

    int FindVertex(const Solid& solid, const glm::vec3& position)
    {
        for (size_t i = 0; i < solid.vertices.size(); ++i)
        {
            if (glm::distance(solid.vertices[i], position) <= kSamePosition) return static_cast<int>(i);
        }
        return -1;
    }

    std::vector<std::pair<uint32_t, uint32_t>> UniqueEdges(const Solid& solid)
    {
        std::set<std::pair<uint32_t, uint32_t>> edges;
        for (const Face& face : solid.faces)
        {
            for (size_t i = 0; i < face.loop.size(); ++i)
            {
                const uint32_t a = face.loop[i];
                const uint32_t b = face.loop[(i + 1) % face.loop.size()];
                edges.insert({ std::min(a, b), std::max(a, b) });
            }
        }
        return { edges.begin(), edges.end() };
    }

    std::string Signed(double value)
    {
        return (value > 0.0 ? "+" : "") + FormatUnits(value);
    }

    std::string DescribeDelta(const glm::dvec3& delta, int axis, bool planeMode)
    {
        if (axis >= 0 && !planeMode) return std::string(1, kAxisNames[axis]) + ": " + Signed(delta[axis]);

        if (axis >= 0)
        {
            const int a = (axis + 1) % 3;
            const int b = (axis + 2) % 3;
            return std::string(1, kAxisNames[std::min(a, b)]) + std::string(1, kAxisNames[std::max(a, b)]) + ": " +
                   Signed(delta[std::min(a, b)]) + ", " + Signed(delta[std::max(a, b)]);
        }
        return "X " + Signed(delta.x) + "  Y " + Signed(delta.y) + "  Z " + Signed(delta.z);
    }
}

// ---------------------------------------------------------------- выбор элементов

bool VertexTool::isSelectedAt(uint64_t solidUid, const glm::vec3& position) const
{
    return std::any_of(m_selected.begin(), m_selected.end(), [&](const Element& e)
    {
        return e.solidUid == solidUid && glm::distance(e.position, position) <= kSamePosition;
    });
}

void VertexTool::addElementsAt(const LevelEditSession& session, const glm::vec3& position)
{
    // Вершину берём у каждого выделенного солида, где она есть: соседние солиды с общим углом двигаются вместе
    for (uint64_t uid : session.selectedSolids())
    {
        const Solid* solid = session.findSolid(uid);
        if (!solid) continue;

        const int index = FindVertex(*solid, position);
        if (index >= 0 && !isSelectedAt(uid, solid->vertices[index])) m_selected.push_back({ uid, solid->vertices[index] });
    }
}

void VertexTool::removeElementsAt(const glm::vec3& position)
{
    m_selected.erase(std::remove_if(m_selected.begin(), m_selected.end(), [&](const Element& e)
    {
        return glm::distance(e.position, position) <= kSamePosition;
    }), m_selected.end());
}

bool VertexTool::hoverIsSelected(const Hover& hover) const
{
    return isSelectedAt(hover.solidUid, hover.a) && (!hover.isEdge || isSelectedAt(hover.solidUid, hover.b));
}

void VertexTool::selectHover(const LevelEditSession& session, const Hover& hover, bool additive)
{
    if (!additive) m_selected.clear();
    addElementsAt(session, hover.a);
    if (hover.isEdge) addElementsAt(session, hover.b);
}

void VertexTool::toggleHover(const LevelEditSession& session, const Hover& hover)
{
    if (hoverIsSelected(hover))
    {
        removeElementsAt(hover.a);
        if (hover.isEdge) removeElementsAt(hover.b);
    }
    else
    {
        selectHover(session, hover, true);
    }
}

void VertexTool::selectAllVertices(const LevelEditSession& session)
{
    m_selected.clear();
    for (uint64_t uid : session.selectedSolids())
    {
        const Solid* solid = session.findSolid(uid);
        if (!solid) continue;
        for (const glm::vec3& v : solid->vertices) m_selected.push_back({ uid, v });
    }
}

VertexTool::Hover VertexTool::pickElement(const LevelEditSession& session, const ViewportPointer& pointer) const
{
    Hover best;
    float bestDistance = (m_mode == Mode::Vertex) ? kVertexPickPixels : kEdgePickPixels;

    for (uint64_t uid : session.selectedSolids())
    {
        const Solid* solid = session.findSolid(uid);
        if (!solid) continue;

        if (m_mode == Mode::Vertex)
        {
            for (const glm::vec3& v : solid->vertices)
            {
                glm::vec2 px;
                if (!ProjectToPixels(pointer, v, px)) continue;

                const float d = glm::length(px - pointer.mousePx);
                if (d < bestDistance) { bestDistance = d; best = { true, false, uid, v, v }; }
            }
        }
        else
        {
            for (const auto& [ia, ib] : UniqueEdges(*solid))
            {
                glm::vec2 pa, pb;
                if (!ProjectToPixels(pointer, solid->vertices[ia], pa) || !ProjectToPixels(pointer, solid->vertices[ib], pb)) continue;

                const float d = PixelDistanceToSegment(pointer.mousePx, pa, pb);
                if (d < bestDistance) { bestDistance = d; best = { true, true, uid, solid->vertices[ia], solid->vertices[ib] }; }
            }
        }
    }
    return best;
}

// ---------------------------------------------------------------- ввод

void VertexTool::onPointer(LevelEditSession& session, uint32_t viewId, const ViewportPointer& pointer)
{
    // Последний указатель над видом нужен клавише G
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

    if (m_grab.active)
    {
        if (viewId == m_grab.view) updateGrab(session, pointer);
        return;
    }

    // Наведение сбрасывает только вид, который его выставил: панелей несколько
    if (!pointer.over || pointer.capturing)
    {
        if (m_hoverView == viewId) m_hover.valid = false;
        return;
    }

    m_hoverView = viewId;
    m_hover = pickElement(session, pointer);

    if (!pointer.pressed) return;

    if (m_hover.valid)
    {
        if (pointer.ctrl)
        {
            toggleHover(session, m_hover);
            return;
        }
        if (!hoverIsSelected(m_hover)) selectHover(session, m_hover, false);

        // Протяжка начинается сразу: клик без смещения остаётся просто выбором
        glm::vec3 anchor = m_hover.a;
        if (m_hover.isEdge)
        {
            glm::vec2 pa, pb;
            if (ProjectToPixels(pointer, m_hover.a, pa) && ProjectToPixels(pointer, m_hover.b, pb) &&
                glm::length(pb - pointer.mousePx) < glm::length(pa - pointer.mousePx))
            {
                anchor = m_hover.b;
            }
        }
        beginGrab(session, viewId, pointer, true, &anchor);
        return;
    }

    // Мимо элементов: клик по телу выбирает солид, пустота снимает выбор вершин
    m_selected.clear();
    const Edit::Ray ray = MakeRay(pointer);
    LevelHit hit;
    if (RaycastLevel(session.document(), glm::vec3(ray.origin), glm::vec3(ray.dir), static_cast<float>(ray.maxDistance), hit))
    {
        session.selectSolid(hit.solidUid, pointer.ctrl);
    }
}

bool VertexTool::onKey(LevelEditSession& session, LevelKey key, bool shift)
{
    switch (key)
    {
        case LevelKey::X:
        case LevelKey::Y:
        case LevelKey::Z:
        {
            if (!m_grab.active) return false;

            const int axis = static_cast<int>(key) - static_cast<int>(LevelKey::X);
            if (m_grab.axis == axis && m_grab.planeMode == shift)
            {
                m_grab.axis = -1;           // повторное нажатие снимает ограничение
                m_grab.planeMode = false;
            }
            else
            {
                m_grab.axis = axis;
                m_grab.planeMode = shift;
            }

            // Ограничение считается от исходного положения мыши: возвращаем форму к исходной, дальше посчитает кадр
            m_grab.delta = glm::dvec3(0.0);
            applyDelta(session, m_grab.delta);
            return true;
        }

        case LevelKey::G:
        {
            if (m_grab.active || m_selected.empty() || !m_hasLast) return false;

            // Якорь: выбранная вершина, ближайшая к курсору на экране
            float bestDistance = 1e30f;
            glm::vec3 anchor(0.0f);
            for (const Element& element : m_selected)
            {
                glm::vec2 px;
                if (!ProjectToPixels(m_last, element.position, px)) continue;

                const float d = glm::length(px - m_last.mousePx);
                if (d < bestDistance) { bestDistance = d; anchor = element.position; }
            }
            if (bestDistance > 1e29f) return false;

            return beginGrab(session, m_lastView, m_last, false, &anchor);
        }

        case LevelKey::Enter:
            if (!m_grab.active || m_grab.fromDrag) return false;
            finishGrab(session);
            return true;

        case LevelKey::Num1:
        case LevelKey::Num2:
            if (m_grab.active) return false;
            m_mode = (key == LevelKey::Num1) ? Mode::Vertex : Mode::Edge;
            m_hover.valid = false;
            return true;

        case LevelKey::SelectAll:
            if (m_grab.active) return false;
            selectAllVertices(session);
            return true;
    }
    return false;
}

void VertexTool::cancel()
{
    m_grab = {};
}

// ---------------------------------------------------------------- перемещение

bool VertexTool::beginGrab(LevelEditSession& session, uint32_t viewId, const ViewportPointer& pointer, bool fromDrag,
                           const glm::vec3* anchorHint)
{
    Grab grab;

    for (uint64_t uid : session.selectedSolids())
    {
        const Solid* solid = session.findSolid(uid);
        if (!solid) continue;

        SolidEdit edit;
        edit.uid = uid;
        edit.source = *solid;
        edit.preview = *solid;

        for (const Element& element : m_selected)
        {
            if (element.solidUid != uid) continue;

            const int index = FindVertex(*solid, element.position);
            if (index >= 0) edit.moved.push_back(static_cast<uint32_t>(index));
        }
        std::sort(edit.moved.begin(), edit.moved.end());
        edit.moved.erase(std::unique(edit.moved.begin(), edit.moved.end()), edit.moved.end());

        if (!edit.moved.empty()) grab.edits.push_back(std::move(edit));
    }
    if (grab.edits.empty() || !anchorHint) return false;

    grab.active = true;
    grab.fromDrag = fromDrag;
    grab.view = viewId;
    grab.pressPx = pointer.mousePx;
    grab.startRay = MakeRay(pointer);
    grab.anchor = glm::dvec3(*anchorHint);
    grab.freeNormal = glm::normalize(glm::dvec3(pointer.camera.forward));
    grab.orthographic = pointer.camera.ortho;

    m_grab = std::move(grab);
    return true;
}

bool VertexTool::computeDelta(const Edit::Ray& ray, double grid, glm::dvec3& out) const
{
    const glm::dvec3& a0 = m_grab.anchor;

    if (m_grab.axis >= 0 && !m_grab.planeMode)
    {
        // Вдоль оси: ближайшая к лучу точка прямой через якорь. База берётся от исходного луча, поэтому смена оси
        // не "прыгает": результат это проекция всего пути мыши на новую ось.
        glm::dvec3 axis(0.0);
        axis[m_grab.axis] = 1.0;

        double t0 = 0.0, t = 0.0;
        if (!Edit::ClosestLineParam(m_grab.startRay, a0, axis, t0) || !Edit::ClosestLineParam(ray, a0, axis, t)) return false;

        const double snapped = Edit::Snap(a0[m_grab.axis] + (t - t0), grid);
        out = axis * (snapped - a0[m_grab.axis]);
        return true;
    }

    glm::dvec3 normal = m_grab.freeNormal;
    if (m_grab.axis >= 0)
    {
        normal = glm::dvec3(0.0);
        normal[m_grab.axis] = 1.0;
    }

    double ts = 0.0, tn = 0.0;
    if (!Edit::RayPlane(m_grab.startRay, a0, normal, ts) || !Edit::RayPlane(ray, a0, normal, tn)) return false;

    out = Edit::SnapVec(a0 + (ray.at(tn) - m_grab.startRay.at(ts)), grid) - a0;

    if (m_grab.axis >= 0)               out[m_grab.axis] = 0.0;                              // плоскость: исключённая ось не двигается
    else if (m_grab.orthographic)       out[Edit::DominantAxis(m_grab.freeNormal)] = 0.0;    // ортографика: глубина не двигается
    return true;
}

bool VertexTool::applyDelta(const LevelEditSession& session, const glm::dvec3& delta)
{
    // Пересобираем все тела во временный набор: если хоть одно недопустимо, предпросмотр остаётся прежним
    std::vector<Solid> rebuilt;
    rebuilt.reserve(m_grab.edits.size());

    for (const SolidEdit& edit : m_grab.edits)
    {
        std::vector<glm::dvec3> positions;
        positions.reserve(edit.source.vertices.size());
        for (const glm::vec3& v : edit.source.vertices) positions.push_back(glm::dvec3(v));
        for (uint32_t index : edit.moved) positions[index] += delta;

        Solid solid;
        std::string error;
        if (!RebuildSolidFromVertices(edit.source, positions, session.document().materials.size(), solid, error)) return false;
        rebuilt.push_back(std::move(solid));
    }

    for (size_t i = 0; i < rebuilt.size(); ++i) m_grab.edits[i].preview = std::move(rebuilt[i]);
    return true;
}

void VertexTool::updateGrab(LevelEditSession& session, const ViewportPointer& pointer)
{
    if (!pointer.capturing)   // во время полёта камеры курсор скрыт: замираем на последней форме
    {
        glm::dvec3 delta;
        if (computeDelta(MakeRay(pointer), session.gridSize(), delta) && delta != m_grab.delta)
        {
            // Недопустимые положения (вершина ушла внутрь тела, объём схлопнулся) пропускаем: остаёмся на последнем допустимом
            if (applyDelta(session, delta)) m_grab.delta = delta;
        }
    }

    if (m_grab.fromDrag ? pointer.released : pointer.pressed) finishGrab(session);
}

void VertexTool::finishGrab(LevelEditSession& session)
{
    Grab grab = std::move(m_grab);
    m_grab = {};

    if (grab.delta == glm::dvec3(0.0)) return;

    std::vector<SolidChange> changes;
    for (SolidEdit& edit : grab.edits)
    {
        changes.push_back(SolidChange::Modified(edit.source, edit.preview));
    }

    if (!session.commit(grab.edits.size() == 1 && grab.edits[0].moved.size() == 1 ? "Move vertex" : "Move vertices", std::move(changes))) return;

    // Выбор следует за вершинами на новые места
    m_selected.clear();
    for (const SolidEdit& edit : grab.edits)
    {
        for (uint32_t index : edit.moved) m_selected.push_back({ edit.uid, edit.preview.vertices[index] });
    }
    m_hover.valid = false;
}

// ---------------------------------------------------------------- подсказка и превью

std::string VertexTool::hint() const
{
    if (m_grab.active)
    {
        return std::string("X / Y / Z: axis, Shift+axis: plane  |  ") + (m_grab.fromDrag ? "release: apply" : "click / Enter: apply") + "  |  Esc: cancel";
    }
    if (m_selected.empty())
    {
        return m_mode == Mode::Vertex ? "Click a vertex (select a solid first)  |  A: all  |  2: edges"
                                      : "Click an edge (select a solid first)  |  A: all vertices  |  1: vertices";
    }
    return "Drag or G: move  |  Ctrl+click: toggle  |  A: all  |  1: vertices  2: edges  |  V: select tool";
}

void VertexTool::collectPreview(const LevelEditSession& session, ToolPreview& out) const
{
    if (m_grab.active)
    {
        for (const SolidEdit& edit : m_grab.edits)
        {
            out.addSolidEdges(edit.preview, kPreviewColor);
            for (uint32_t index : edit.moved) out.addPoint(edit.preview.vertices[index], kSelectedColor, 9.0f);
        }

        // Направляющие осей через исходный якорь
        const glm::vec3 anchor(m_grab.anchor);
        if (m_grab.axis >= 0)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                const bool drawn = m_grab.planeMode ? axis != m_grab.axis : axis == m_grab.axis;
                if (!drawn) continue;

                glm::vec3 direction(0.0f);
                direction[axis] = static_cast<float>(kGuideLength);
                out.addLine(anchor - direction, anchor + direction, kAxisColors[axis]);
            }
        }

        out.addText(anchor + glm::vec3(m_grab.delta), DescribeDelta(m_grab.delta, m_grab.axis, m_grab.planeMode), kHoverColor);
        return;
    }

    for (uint64_t uid : session.selectedSolids())
    {
        const Solid* solid = session.findSolid(uid);
        if (!solid) continue;

        // Рёбра, оба конца которых выбраны, подсвечиваем (и в режиме вершин тоже)
        for (const auto& [ia, ib] : UniqueEdges(*solid))
        {
            if (isSelectedAt(uid, solid->vertices[ia]) && isSelectedAt(uid, solid->vertices[ib]))
                out.addLine(solid->vertices[ia], solid->vertices[ib], kSelectedColor);
        }

        for (const glm::vec3& v : solid->vertices)
        {
            const bool selected = isSelectedAt(uid, v);
            out.addPoint(v, selected ? kSelectedColor : kVertexColor, selected ? 9.0f : 6.0f);
        }
    }

    if (m_hover.valid)
    {
        if (m_hover.isEdge) out.addLine(m_hover.a, m_hover.b, kHoverColor);
        out.addPoint(m_hover.a, kHoverColor, 10.0f);
        if (m_hover.isEdge) out.addPoint(m_hover.b, kHoverColor, 10.0f);
    }
}
