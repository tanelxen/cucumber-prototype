//
//  LevelEditOverlay.cpp
//  Cucumber
//

#include "LevelEditOverlay.h"

#include "GizmoRender.h"
#include "level/LevelEditSession.h"

namespace
{
    const glm::vec4 kSelectedSolid{1.0f, 0.60f, 0.10f, 1.0f};
    const glm::vec4 kSelectedFace {1.0f, 0.95f, 0.20f, 1.0f};
    const glm::vec4 kHoverSolid   {0.6f, 0.90f, 1.00f, 0.8f};
    const glm::vec4 kHoverFace    {0.3f, 1.00f, 1.00f, 1.0f};

    // Каждое ребро принадлежит двум граням в противоположных направлениях: рисуем только одно из них
    void DrawSolidEdges(const Solid& solid, const glm::vec4& color)
    {
        GizmoRender& gizmo = GizmoRender::Get();
        for (const Face& face : solid.faces)
        {
            for (size_t i = 0; i < face.loop.size(); ++i)
            {
                const uint32_t a = face.loop[i];
                const uint32_t b = face.loop[(i + 1) % face.loop.size()];
                if (a < b) gizmo.DrawLine(solid.vertices[a], solid.vertices[b], color);
            }
        }
    }

    // Контур грани чуть приподнят над поверхностью, чтобы не сливаться с рёбрами солида
    void DrawFaceOutline(const Solid& solid, const Face& face, const glm::vec4& color)
    {
        GizmoRender& gizmo = GizmoRender::Get();
        const glm::vec3 lift = FaceNormal(solid, face) * 0.1f;
        for (size_t i = 0; i < face.loop.size(); ++i)
        {
            gizmo.DrawLine(solid.vertices[face.loop[i]] + lift,
                           solid.vertices[face.loop[(i + 1) % face.loop.size()]] + lift, color);
        }
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

    if (session.hasHover())
    {
        const LevelHit& hit = session.hover();
        if (const Solid* solid = session.findSolid(hit.solidUid))
        {
            if (!session.isSolidSelected(hit.solidUid)) DrawSolidEdges(*solid, kHoverSolid);
            if (const Face* face = FindFace(*solid, hit.faceUid)) DrawFaceOutline(*solid, *face, kHoverFace);
        }
    }
}
