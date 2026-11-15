//
//  ViewportToolbar.cpp
//  Cucumber
//

#include "ViewportToolbar.h"

#include "EditorViewport.h"
#include "EditorWidgets.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace
{
    constexpr float kGridSteps[] = { 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 64.0f, 128.0f, 256.0f };

    void DrawGridSizeCombo(float& gridSize)
    {
        char preview[16];
        std::snprintf(preview, sizeof(preview), "%g", gridSize);

        ImGui::SetNextItemWidth(64);
        if (ImGui::BeginCombo("##GridSize", preview))
        {
            for (float step : kGridSteps)
            {
                char label[16];
                std::snprintf(label, sizeof(label), "%g", step);
                if (ImGui::Selectable(label, step == gridSize)) gridSize = step;
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Grid size  ( [ / ] )");
    }

    // [ и ]: шаг вдвое меньше / больше
    void HandleGridSizeKeys(float& gridSize)
    {
        if (ImGui::GetIO().WantTextInput) return;

        if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket, false)) gridSize = std::max(1.0f, gridSize * 0.5f);
        if (ImGui::IsKeyPressed(ImGuiKey_RightBracket, false)) gridSize = std::min(1024.0f, gridSize * 2.0f);
    }
}

ToolbarAction DrawViewportToolbar(EditorViewport& viewport, float& gridSize,
                                  bool showGridSize, bool focused, bool simulating)
{
    EditorCamera& camera = viewport.camera();

    ImGui::Checkbox("Grid", &viewport.drawGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Gizmos", &viewport.drawGizmos);
    ImGui::SameLine();

    int mode = static_cast<int>(camera.mode());
    ImGui::SetNextItemWidth(110);
    if (ImGui::Combo("##ViewMode", &mode, "Perspective\0Top\0Front\0Side\0"))
    {
        camera.setMode(static_cast<EditorViewMode>(mode));

        // Как в TrenchBroom / Hammer: ортографика по умолчанию каркасная, перспектива закрашенная
        // (0 = Perspective, порядок как в комбо). Строку можно удалить, если режим должен быть независим от проекции.
        viewport.shadeMode = (mode == 0) ? ShadeMode::Shaded : ShadeMode::Wireframe;
    }

    ImGui::SameLine();
    int shading = static_cast<int>(viewport.shadeMode);
    ImGui::SetNextItemWidth(100);
    if (ImGui::Combo("##ShadeMode", &shading, "Shaded\0Wireframe\0"))
    {
        viewport.shadeMode = static_cast<ShadeMode>(shading);
    }

    if (showGridSize)
    {
        ImGui::SameLine();
        DrawGridSizeCombo(gridSize);
        if (focused) HandleGridSizeKeys(gridSize);
    }

    ImGui::SameLine();
    ImGui::TextDisabled("RMB: Fly / Pan");

    ImGui::SameLine(ImGui::GetWindowWidth() - 48);

    if (simulating)
    {
        if (EditorWidgets::StopButton()) return ToolbarAction::Stop;
    }
    else if (EditorWidgets::PlayButton())
    {
        return ToolbarAction::Play;
    }

    return ToolbarAction::None;
}
