//
//  LevelEditOverlay.cpp
//  Cucumber
//

#include "LevelEditOverlay.h"

#include "GizmoRender.h"
#include "level/LevelEditSession.h"

#include <imgui.h>

namespace
{
    const glm::vec4 kSelectedSolid{1.0f, 0.60f, 0.10f, 1.0f};
    const glm::vec4 kSelectedFace {1.0f, 0.95f, 0.20f, 1.0f};
    const glm::vec4 kHoverSolid   {0.6f, 0.90f, 1.00f, 0.8f};
    const glm::vec4 kHoverFace    {0.3f, 1.00f, 1.00f, 1.0f};

    void DrawSolidEdges(const Solid& solid, const glm::vec4& color)
    {
        ToolPreview preview;
        preview.addSolidEdges(solid, color);
        for (const ToolPreview::Line& line : preview.lines) GizmoRender::Get().DrawLine(line.a, line.b, line.color);
    }

    void DrawFaceOutline(const Solid& solid, const Face& face, const glm::vec4& color)
    {
        ToolPreview preview;
        preview.addFaceOutline(solid, face, color);
        for (const ToolPreview::Line& line : preview.lines) GizmoRender::Get().DrawLine(line.a, line.b, line.color);
    }

    const Face* FindFace(const Solid& solid, uint64_t uid)
    {
        for (const Face& face : solid.faces)
        {
            if (face.uid == uid) return &face;
        }
        return nullptr;
    }

    ImU32 ToColor(const glm::vec4& c)
    {
        return IM_COL32(int(c.x * 255.0f), int(c.y * 255.0f), int(c.z * 255.0f), int(c.w * 255.0f));
    }
}

void DrawLevelEditOverlay(const LevelEditSession& session)
{
    for (uint64_t uid : session.selectedSolids())
    {
        if (const Solid* solid = session.findSolid(uid)) DrawSolidEdges(*solid, kSelectedSolid);
    }

    if (session.selectedFaceUid() != 0)
    {
        if (const Solid* solid = session.findSolid(session.selectedFaceSolidUid()))
        {
            if (const Face* face = FindFace(*solid, session.selectedFaceUid())) DrawFaceOutline(*solid, *face, kSelectedFace);
        }
    }

    if (session.hasHover() && session.activeTool().showsHover())
    {
        const LevelHit& hit = session.hover();
        if (const Solid* solid = session.findSolid(hit.solidUid))
        {
            if (!session.isSolidSelected(hit.solidUid)) DrawSolidEdges(*solid, kHoverSolid);
            if (const Face* face = FindFace(*solid, hit.faceUid)) DrawFaceOutline(*solid, *face, kHoverFace);
        }
    }

    ToolPreview preview;
    session.activeTool().collectPreview(session, preview);
    for (const ToolPreview::Line& line : preview.lines) GizmoRender::Get().DrawLine(line.a, line.b, line.color);
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
        if (clip.w <= 1e-4f) continue;   // позади камеры (в ортографии w = 1)

        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (ndc.x < -1.0f || ndc.x > 1.0f || ndc.y < -1.0f || ndc.y > 1.0f) continue;

        const ImVec2 size = ImGui::CalcTextSize(label.text.c_str());
        const ImVec2 position(x + (ndc.x * 0.5f + 0.5f) * width - size.x * 0.5f,
                              y + (1.0f - (ndc.y * 0.5f + 0.5f)) * height - size.y * 0.5f);

        drawList->AddRectFilled(ImVec2(position.x - 4, position.y - 2), ImVec2(position.x + size.x + 4, position.y + size.y + 2),
                                IM_COL32(0, 0, 0, 160), 3.0f);
        drawList->AddText(position, ToColor(label.color), label.text.c_str());
    }
}
