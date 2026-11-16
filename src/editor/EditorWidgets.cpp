//
//  EditorWidgets.cpp
//  Cucumber
//
//  DrawPlayButton и DrawStopButton были копиями друг друга и отличались цветами и иконкой.
//

#include "EditorWidgets.h"

#include <imgui.h>

namespace
{
    struct TransportStyle
    {
        ImVec4 background;
        ImVec4 hovered;
        ImVec4 active;
        ImVec4 border;
        ImVec4 text;
        ImVec4 icon;
    };

    constexpr float kButtonSize = 32.0f;
    constexpr float kIconSize = 12.0f;

    template <typename DrawIcon>
    bool TransportButton(const char* id, const TransportStyle& style, DrawIcon drawIcon)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, style.background);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, style.hovered);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, style.active);
        ImGui::PushStyleColor(ImGuiCol_Border, style.border);
        ImGui::PushStyleColor(ImGuiCol_Text, style.text);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

        const bool pressed = ImGui::Button(id, ImVec2(kButtonSize, kButtonSize));

        // Иконка рисуется по центру только что нарисованной кнопки
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const ImVec2 iconMin(min.x + (max.x - min.x - kIconSize) * 0.5f,
                             min.y + (max.y - min.y - kIconSize) * 0.5f);
        drawIcon(ImGui::GetWindowDrawList(), iconMin, ImGui::ColorConvertFloat4ToU32(style.icon));

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(5);

        return pressed;
    }
}

namespace EditorWidgets
{
    bool PlayButton(const char* id)
    {
        const TransportStyle style{
            ImVec4(0.08f, 0.25f, 0.14f, 1.0f),   // фон
            ImVec4(0.12f, 0.33f, 0.18f, 1.0f),   // наведение
            ImVec4(0.05f, 0.18f, 0.10f, 1.0f),   // нажатие
            ImVec4(0.12f, 0.33f, 0.18f, 1.0f),   // рамка
            ImVec4(0.85f, 0.95f, 0.88f, 1.0f),   // текст
            ImVec4(0.30f, 0.85f, 0.45f, 1.0f)    // иконка
        };

        return TransportButton(id, style, [](ImDrawList* drawList, const ImVec2& iconMin, ImU32 color)
        {
            // Треугольник Play, сдвинутый на пиксель вправо для оптического центра
            const float left = iconMin.x + 1.0f;
            const ImVec2 p1(left, iconMin.y);
            const ImVec2 p2(left, iconMin.y + kIconSize);
            const ImVec2 p3(left + kIconSize * 0.85f, iconMin.y + kIconSize * 0.5f);
            drawList->AddTriangleFilled(p1, p2, p3, color);
        });
    }

    bool StopButton(const char* id)
    {
        const TransportStyle style{
            ImVec4(0.28f, 0.08f, 0.08f, 1.0f),
            ImVec4(0.38f, 0.12f, 0.12f, 1.0f),
            ImVec4(0.20f, 0.05f, 0.05f, 1.0f),
            ImVec4(0.38f, 0.12f, 0.12f, 1.0f),
            ImVec4(0.95f, 0.85f, 0.85f, 1.0f),
            ImVec4(0.85f, 0.30f, 0.30f, 1.0f)
        };

        return TransportButton(id, style, [](ImDrawList* drawList, const ImVec2& iconMin, ImU32 color)
        {
            drawList->AddRectFilled(iconMin, ImVec2(iconMin.x + kIconSize, iconMin.y + kIconSize), color, 2.0f);
        });
    }
}
