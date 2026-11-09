//
//  LevelEditOverlay.cpp
//  Cucumber
//

#include "LevelEditOverlay.h"

#include "level/LevelEditSession.h"
#include "renderer/OverlayRender.h"

#include <imgui.h>

namespace
{
    // Рёбра выбранного солида рисуются без теста глубины: видны все, в том числе тыльные и закрытые другими солидами
    // (так удобно при push/pull). Depth::XRay сделал бы скрытые части приглушёнными, Depth::Tested как обычную геометрию.
    constexpr OverlayRender::Depth kSelectionDepth = OverlayRender::Depth::XRay;

    constexpr float kSelectedWidth = 1.0f;
    constexpr float kFaceWidth = 1.0f;

    const glm::vec4 kSelectedSolid{1.0f, 0.10f, 0.10f, 1.0f};
    const glm::vec4 kSelectedFace {1.0f, 0.95f, 0.20f, 1.0f};
    const glm::vec4 kHoverSolid   {0.6f, 0.90f, 1.00f, 0.8f};
    const glm::vec4 kHoverFace    {0.3f, 1.00f, 1.00f, 1.0f};

    OverlayRender::Depth ToDepth(bool onTop)
    {
        return onTop ? OverlayRender::Depth::Always : OverlayRender::Depth::Tested;
    }

    OverlayRender::Shape ToShape(ToolPreview::Shape shape)
    {
        switch (shape)
        {
            case ToolPreview::Shape::Diamond: return OverlayRender::Shape::Diamond;
            case ToolPreview::Shape::Circle:  return OverlayRender::Shape::Circle;
            default:                          return OverlayRender::Shape::Square;
        }
    }

    void Submit(const ToolPreview& preview, OverlayRender::Depth overrideDepth, bool useOverride)
    {
        OverlayRender& overlay = OverlayRender::Get();
        for (const ToolPreview::Line& line : preview.lines)
            overlay.DrawLine(line.a, line.b, line.color, line.width, useOverride ? overrideDepth : ToDepth(line.onTop));
        for (const ToolPreview::Point& point : preview.points)
            overlay.DrawPoint(point.position, point.color, point.sizePx, ToShape(point.shape), OverlayRender::Depth::Always);
    }

    const Face* FindFace(const Solid& solid, uint64_t uid)
    {
        for (const Face& face : solid.faces)
        {
            if (face.uid == uid) return &face;
        }
        return nullptr;
    }
}

void DrawLevelEditOverlay(const LevelEditSession& session)
{
    // Выбранные солиды: все рёбра, без теста глубины
    for (uint64_t uid : session.selectedSolids())
    {
        if (const Solid* solid = session.findSolid(uid))
        {
            ToolPreview preview;
            preview.addSolidEdges(*solid, kSelectedSolid, kSelectedWidth);
            Submit(preview, kSelectionDepth, true);
        }
    }

    // Выбранные грани: контур потолще
    for (const FaceRef& ref : session.selectedFaces())
    {
        const Solid* solid = session.findSolid(ref.solidUid);
        const Face* face = solid ? FindFace(*solid, ref.faceUid) : nullptr;
        if (!face) continue;

        ToolPreview preview;
        preview.addFaceOutline(*solid, *face, kSelectedFace, 0.1f, kFaceWidth);
        Submit(preview, kSelectionDepth, true);
    }

    // Подсветка солида под курсором выключена по умолчанию (см. LevelEditSession::hoverHighlight): лишний визуальный шум
    if (session.hoverHighlight() && session.hasHover() && session.activeTool().showsHover())
    {
        const LevelHit& hit = session.hover();
        if (const Solid* solid = session.findSolid(hit.solidUid))
        {
            ToolPreview preview;
            if (!session.isSolidSelected(hit.solidUid)) preview.addSolidEdges(*solid, kHoverSolid, 1.5f);
            if (const Face* face = FindFace(*solid, hit.faceUid)) preview.addFaceOutline(*solid, *face, kHoverFace, 0.1f, kFaceWidth);
            Submit(preview, kSelectionDepth, true);
        }
    }

    // Превью и ручки активного инструмента: у каждой линии своя толщина и режим глубины
    ToolPreview preview;
    session.activeTool().collectPreview(session, preview);
    Submit(preview, OverlayRender::Depth::Always, false);
}

void DrawLevelEditLabels(const LevelEditSession& session, const CameraData& camera, float x, float y, float width, float height)
{
    ToolPreview preview;
    session.activeTool().collectPreview(session, preview);
    if (preview.texts.empty()) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const glm::mat4 viewProj = camera.viewProj();

    for (const ToolPreview::Text& label : preview.texts)
    {
        const glm::vec4 clip = viewProj * glm::vec4(label.position, 1.0f);
        if (clip.w <= 1e-4f) continue;   // позади камеры (в ортографике w = 1)

        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (ndc.x < -1.0f || ndc.x > 1.0f || ndc.y < -1.0f || ndc.y > 1.0f) continue;

        const ImVec2 size = ImGui::CalcTextSize(label.text.c_str());
        const ImVec2 position(x + (ndc.x * 0.5f + 0.5f) * width - size.x * 0.5f,
                              y + (1.0f - (ndc.y * 0.5f + 0.5f)) * height - size.y * 0.5f);

        const ImU32 color = IM_COL32(int(label.color.x * 255.0f), int(label.color.y * 255.0f),
                                     int(label.color.z * 255.0f), int(label.color.w * 255.0f));
        drawList->AddRectFilled(ImVec2(position.x - 4, position.y - 2), ImVec2(position.x + size.x + 4, position.y + size.y + 2),
                                IM_COL32(0, 0, 0, 160), 3.0f);
        drawList->AddText(position, color, label.text.c_str());
    }
}
