//
//  LevelEditInput.cpp
//  Cucumber
//

#include "LevelEditInput.h"

#include "EditorViewport.h"

#include "level/LevelEditSession.h"

#include <imgui.h>

#include <iterator>

namespace
{
    // Any        любые модификаторы
    // NoCommand  Ctrl / Cmd не зажат (Shift игнорируется)
    // Command    Ctrl / Cmd зажат
    // ShiftOnly  Shift без Ctrl / Cmd
    enum class Mods { Any, NoCommand, Command, ShiftOnly };

    using Result = LevelEditInput::ShortcutResult;
    using Action = void (*)(LevelEditSession& edit, bool shift, Result& result);

    struct Binding
    {
        ImGuiKey key;
        Mods mods;
        Action action;
    };

    // Клавиши инструментов, осей и элементов. Порядок не важен: один кадр нажимает несколько клавиш редко,
    // а каждая привязка независима.
    const Binding kBindings[] = {
        // Инструменты
        { ImGuiKey_B, Mods::NoCommand, [](LevelEditSession& e, bool, Result&) { e.setTool(ToolType::Block); } },
        { ImGuiKey_V, Mods::NoCommand, [](LevelEditSession& e, bool, Result&) { e.setTool(ToolType::Select); } },
        { ImGuiKey_E, Mods::NoCommand, [](LevelEditSession& e, bool, Result&) { e.setTool(ToolType::Vertex); } },

        { ImGuiKey_Escape,    Mods::Any, [](LevelEditSession& e, bool, Result&) { e.escape(); } },
        { ImGuiKey_Delete,    Mods::Any, [](LevelEditSession& e, bool, Result&) { e.deleteSelection(); } },
        { ImGuiKey_Backspace, Mods::Any, [](LevelEditSession& e, bool, Result&) { e.deleteSelection(); } },

        // История и сохранение
        { ImGuiKey_Z, Mods::Command, [](LevelEditSession& e, bool shift, Result&) { if (shift) e.redo(); else e.undo(); } },
        { ImGuiKey_Y, Mods::Command, [](LevelEditSession& e, bool, Result&) { e.redo(); } },
        { ImGuiKey_S, Mods::Command, [](LevelEditSession&, bool, Result& r) { r.saveRequested = true; } },

        // Оси, перемещение, режим элементов (Ctrl+Z / Ctrl+Y остаются за историей, поэтому NoCommand)
        { ImGuiKey_X, Mods::NoCommand, [](LevelEditSession& e, bool shift, Result&) { e.handleKey(LevelKey::X, shift); } },
        { ImGuiKey_Y, Mods::NoCommand, [](LevelEditSession& e, bool shift, Result&) { e.handleKey(LevelKey::Y, shift); } },
        { ImGuiKey_Z, Mods::NoCommand, [](LevelEditSession& e, bool shift, Result&) { e.handleKey(LevelKey::Z, shift); } },
        { ImGuiKey_G, Mods::NoCommand, [](LevelEditSession& e, bool, Result&) { e.handleKey(LevelKey::G, false); } },
        { ImGuiKey_A, Mods::NoCommand, [](LevelEditSession& e, bool, Result&) { e.handleKey(LevelKey::SelectAll, false); } },
        { ImGuiKey_1, Mods::NoCommand, [](LevelEditSession& e, bool, Result&) { e.handleKey(LevelKey::Num1, false); } },
        { ImGuiKey_2, Mods::NoCommand, [](LevelEditSession& e, bool, Result&) { e.handleKey(LevelKey::Num2, false); } },
        { ImGuiKey_Enter,       Mods::NoCommand, [](LevelEditSession& e, bool, Result&) { e.handleKey(LevelKey::Enter, false); } },
        { ImGuiKey_KeypadEnter, Mods::NoCommand, [](LevelEditSession& e, bool, Result&) { e.handleKey(LevelKey::Enter, false); } },

        // Буфер обмена и дублирование
        { ImGuiKey_C, Mods::Command, [](LevelEditSession& e, bool, Result&) { e.handleKey(LevelKey::Copy, false); } },
        { ImGuiKey_X, Mods::Command, [](LevelEditSession& e, bool, Result&) { e.handleKey(LevelKey::Cut, false); } },
        { ImGuiKey_V, Mods::Command, [](LevelEditSession& e, bool, Result&) { e.handleKey(LevelKey::Paste, false); } },
        { ImGuiKey_D, Mods::Command,   [](LevelEditSession& e, bool, Result&) { e.handleKey(LevelKey::Duplicate, false); } },
        { ImGuiKey_D, Mods::ShiftOnly, [](LevelEditSession& e, bool, Result&) { e.handleKey(LevelKey::DuplicateMove, true); } },
    };

    bool Matches(Mods mods, bool command, bool shift)
    {
        switch (mods)
        {
            case Mods::Any:       return true;
            case Mods::NoCommand: return !command;
            case Mods::Command:   return command;
            case Mods::ShiftOnly: return !command && shift;
        }
        return false;
    }
}

namespace LevelEditInput
{
    ViewportPointer MakePointer(EditorViewport& viewport, const ViewportImageRect& image,
                                const glm::vec2& mouse, bool hoverInput,
                                const Input::State& physicalInput)
    {
        const ImGuiIO& io = ImGui::GetIO();
        EditorCamera& camera = viewport.camera();

        ViewportPointer pointer;
        pointer.camera   = camera.build(viewport.aspect());
        pointer.sizePx   = image.size();                       // не меньше 1x1
        pointer.mousePx  = image.toLocal(mouse.x, mouse.y);
        pointer.capturing = camera.isCapturingMouse();
        pointer.over     = hoverInput && !pointer.capturing;
        pointer.down     = physicalInput.isLeftMouseButtonPressed();
        pointer.pressed  = physicalInput.isLeftMouseButtonJustPressed();
        pointer.released = physicalInput.isLeftMouseButtonClicked();
        pointer.ctrl     = io.KeyCtrl || io.KeySuper;          // Cmd на macOS
        pointer.shift    = io.KeyShift;
        pointer.alt      = io.KeyAlt;
        return pointer;
    }

    ShortcutResult HandleShortcuts(LevelEditSession& edit)
    {
        ShortcutResult result;

        const ImGuiIO& io = ImGui::GetIO();
        if (io.WantTextInput) return result;

        const bool command = io.KeyCtrl || io.KeySuper;
        const bool shift = io.KeyShift;

        for (const Binding& binding : kBindings)
        {
            if (!Matches(binding.mods, command, shift)) continue;
            if (!ImGui::IsKeyPressed(binding.key, false)) continue;

            binding.action(edit, shift, result);
        }
        return result;
    }
}
