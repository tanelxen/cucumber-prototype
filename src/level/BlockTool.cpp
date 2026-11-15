//
//  BlockTool.cpp
//  Cucumber
//

#include "BlockTool.h"

#include "LevelEditSession.h"
#include "SolidBuilder.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kDragPixels = 4.0f;   // насколько надо увести курсор, чтобы нажатие считалось протяжкой, а не кликом

    const glm::vec4 kBaseColor{0.3f, 0.9f, 1.0f, 1.0f};
    const glm::vec4 kBoxColor{1.0f, 0.95f, 0.2f, 1.0f};
    const glm::vec4 kCandidateColor{0.3f, 0.9f, 1.0f, 0.9f};
}

void BlockTool::cancel()
{
    m_stage = Stage::Idle;
    m_dragging = false;
    m_hasCandidate = false;
}

void BlockTool::onPointer(LevelEditSession& session, uint32_t viewId, const ViewportPointer& pointer)
{
    switch (m_stage)
    {
        case Stage::Idle:   updateIdle(session, viewId, pointer); break;
        case Stage::Base:   if (viewId == m_view && !pointer.capturing) updateBase(session, pointer); break;
        case Stage::Height: if (viewId == m_view && !pointer.capturing) updateHeight(session, pointer); break;
    }
}

bool BlockTool::choosePlane(const LevelEditSession& session, const ViewportPointer& pointer, const Edit::Ray& ray,
                            glm::dvec3& outPoint, glm::dvec3& outNormal) const
{
    double t = 0.0;

    if (pointer.camera.ortho)
    {
        // Плоскость перпендикулярна оси взгляда, на глубине последнего бокса; нормаль смотрит на наблюдателя
        const glm::dvec3 forward(pointer.camera.forward);
        const int axis = Edit::DominantAxis(forward);

        outNormal = glm::dvec3(0.0);
        outNormal[axis] = forward[axis] > 0.0 ? -1.0 : 1.0;

        outPoint = glm::dvec3(0.0);
        outPoint[axis] = m_lastPlaneCoord[axis];

        if (!Edit::RayPlane(ray, outPoint, outNormal, t)) return false;
        outPoint = ray.at(t);
        return true;
    }

    LevelHit hit;
    if (RaycastLevel(session.document(), glm::vec3(ray.origin), glm::vec3(ray.dir), static_cast<float>(ray.maxDistance), hit))
    {
        // Бокс осевой, поэтому наклонную грань заменяем ближайшей осевой плоскостью через точку попадания
        outNormal = Edit::AxisNormal(glm::dvec3(hit.normal));
        outPoint = glm::dvec3(hit.position);
        return true;
    }

    outNormal = { 0.0, 1.0, 0.0 };   // "земля" y = 0
    if (!Edit::RayPlane(ray, glm::dvec3(0.0), outNormal, t)) return false;
    outPoint = ray.at(t);
    return true;
}

void BlockTool::updateIdle(LevelEditSession& session, uint32_t viewId, const ViewportPointer& pointer)
{
    // Кандидата сбрасывает только вид, который его выставил: панелей несколько, и соседняя без курсора не должна его гасить
    if (!pointer.over || pointer.capturing)
    {
        if (m_candidateView == viewId) m_hasCandidate = false;
        return;
    }

    m_hasCandidate = false;
    m_candidateView = viewId;

    const double grid = session.gridSize();
    const Edit::Ray ray = MakeRay(pointer);

    glm::dvec3 point, normal;
    if (!choosePlane(session, pointer, ray, point, normal)) return;

    m_candidate = Edit::SnapVec(point, grid);
    m_normal = normal;
    m_axis = Edit::DominantAxis(normal);
    m_hasCandidate = true;

    if (!pointer.pressed) return;

    m_stage = Stage::Base;
    m_view = viewId;
    m_ortho = pointer.camera.ortho;
    m_anchor = m_candidate;
    m_corner = m_candidate;
    m_pressPx = pointer.mousePx;
    m_dragging = false;
    m_hasCandidate = false;
}

bool BlockTool::baseIsValid(double grid) const
{
    for (int axis = 0; axis < 3; ++axis)
    {
        if (axis == m_axis) continue;
        if (std::abs(m_corner[axis] - m_anchor[axis]) < grid * 0.5) return false;
    }
    return true;
}

void BlockTool::boxExtents(double height, glm::dvec3& mins, glm::dvec3& maxs) const
{
    mins = glm::min(m_anchor, m_corner);
    maxs = glm::max(m_anchor, m_corner);

    // Вдоль оси нормали: от плоскости основания на height в сторону нормали (знак компоненты нормали учтён)
    const double c0 = m_anchor[m_axis];
    const double c1 = c0 + m_normal[m_axis] * height;
    mins[m_axis] = std::min(c0, c1);
    maxs[m_axis] = std::max(c0, c1);
}

void BlockTool::updateBase(LevelEditSession& session, const ViewportPointer& pointer)
{
    const double grid = session.gridSize();
    const Edit::Ray ray = MakeRay(pointer);

    double t = 0.0;
    if (Edit::RayPlane(ray, m_anchor, m_normal, t))
    {
        glm::dvec3 corner = Edit::SnapVec(ray.at(t), grid);
        corner[m_axis] = m_anchor[m_axis];
        m_corner = corner;
    }

    if (pointer.down && !m_dragging && glm::length(pointer.mousePx - m_pressPx) > kDragPixels) m_dragging = true;

    // Второй угол фиксируется кликом или отпусканием после протяжки
    const bool finalize = pointer.pressed || (pointer.released && m_dragging);
    if (!finalize) return;

    if (!baseIsValid(grid))
    {
        m_dragging = false;   // нулевая площадь: ждём следующего клика
        return;
    }

    if (m_ortho)
    {
        commitBox(session, m_lastExtent[m_axis]);
        return;
    }

    m_stage = Stage::Height;
    m_dragging = false;
    m_height = std::max(grid, m_lastExtent[m_axis]);
}

void BlockTool::updateHeight(LevelEditSession& session, const ViewportPointer& pointer)
{
    const double grid = session.gridSize();

    // Высота = ближайшая к лучу точка прямой, идущей от угла основания вдоль нормали
    double t = 0.0;
    if (Edit::ClosestLineParam(MakeRay(pointer), m_corner, m_normal, t)) m_height = Edit::Snap(t, grid);

    if (pointer.pressed && std::abs(m_height) >= grid * 0.5) commitBox(session, m_height);
}

bool BlockTool::commitBox(LevelEditSession& session, double height)
{
    glm::dvec3 mins, maxs;
    boxExtents(height, mins, maxs);

    Solid solid;
    std::string error;
    const uint32_t material = session.currentMaterial();   // до построения: может добавить "default" в таблицу
    if (!MakeBoxSolid(mins, maxs, material, session.document().materials.size(), solid, error))
    {
        session.reportError(error);
        cancel();
        return false;
    }

    const uint64_t uid = solid.uid;
    const int axis = m_axis;
    const double planeCoord = m_anchor[axis];

    cancel();
    if (!session.commit("Create box", { SolidChange::Created(std::move(solid)) })) return false;

    m_lastExtent[axis] = std::max(std::abs(height), session.gridSize());
    m_lastPlaneCoord[axis] = planeCoord;
    session.selectSolid(uid, false);
    return true;
}

std::string BlockTool::hint() const
{
    switch (m_stage)
    {
        case Stage::Idle:   return "Click to place the first corner  |  Esc: back to Select";
        case Stage::Base:   return m_ortho ? "Move and click to create the box (depth = last box)  |  Esc: cancel"
                                           : "Move and click to place the second corner  |  Esc: cancel";
        case Stage::Height: return "Move to set the height, click to create the box  |  Esc: cancel";
    }
    return {};
}

void BlockTool::collectPreview(const LevelEditSession& session, ToolPreview& out) const
{
    const double grid = session.gridSize();

    if (m_stage == Stage::Idle)
    {
        if (!m_hasCandidate) return;

        const float arm = static_cast<float>(std::max(grid, 8.0) * 0.5);
        const glm::vec3 c(m_candidate);
        out.addLine(c - glm::vec3(arm, 0, 0), c + glm::vec3(arm, 0, 0), kCandidateColor);
        out.addLine(c - glm::vec3(0, arm, 0), c + glm::vec3(0, arm, 0), kCandidateColor);
        out.addLine(c - glm::vec3(0, 0, arm), c + glm::vec3(0, 0, arm), kCandidateColor);
        return;
    }

    const double height = (m_stage == Stage::Height) ? m_height : (m_ortho ? m_lastExtent[m_axis] : 0.0);

    glm::dvec3 mins, maxs;
    boxExtents(height, mins, maxs);
    out.addBox(mins, maxs, m_stage == Stage::Base ? kBaseColor : kBoxColor);

    const glm::dvec3 size = maxs - mins;
    std::string label = FormatUnits(size.x) + " x " + FormatUnits(size.y) + " x " + FormatUnits(size.z);
    out.addText(glm::vec3((mins + maxs) * 0.5 + glm::dvec3(0.0, 0.0, 0.0)), std::move(label), kBoxColor);
}
