//
//  ViewportGizmo.cpp
//  Cucumber
//

#include "ViewportGizmo.h"

#include "Entity.h"

#include <ImGuizmo.h>

namespace ViewportGizmo
{
    void Draw(Entity* selected, const CameraData& camera, const ImVec2& contentPos,
              const ImVec2& contentSize, int id, bool enabled, float gridStep)
    {
        ImGuizmo::AllowAxisFlip(false);

        if (!selected) return;

        ImGuizmo::SetOrthographic(camera.ortho);
        ImGuizmo::BeginFrame();
        ImGuizmo::Enable(enabled);
        ImGuizmo::SetDrawlist();

        ImGuizmo::PushID(id);
        ImGuizmo::SetRect(contentPos.x, contentPos.y, contentSize.x, contentSize.y);

        auto& transform = selected->getTransform();
        glm::mat4 worldMatrix = transform.getWorldMatrix();

        const float* viewPtr = &camera.view[0][0];
        const float* projPtr = &camera.proj[0][0];
        float* matrixPtr = &worldMatrix[0][0];

        float snap[3] = { gridStep, gridStep, gridStep };
        ImGuizmo::Manipulate(viewPtr, projPtr, ImGuizmo::TRANSLATE, ImGuizmo::LOCAL, matrixPtr, nullptr, snap);

        if (ImGuizmo::IsUsing())
        {
            transform.setWorldMatrix(worldMatrix);
        }

        ImGuizmo::PopID();
    }
}
