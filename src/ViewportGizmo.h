//
//  ViewportGizmo.h
//  Cucumber
//
//  Гизмо перемещения выбранной сущности (ImGuizmo).
//

#pragma once

#include "ViewDesc.h"   // CameraData

#include <imgui.h>

class Entity;

namespace ViewportGizmo
{
    // gridStep: шаг привязки при перемещении. Раньше был зашит 8, независимо от настроенной сетки.
    void Draw(Entity* selected, const CameraData& camera, const ImVec2& contentPos,
              const ImVec2& contentSize, int id, bool enabled, float gridStep);
}
