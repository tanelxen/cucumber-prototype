//
//  LevelEditInput.h
//  Cucumber
//
//  Переводит ввод ImGui в вызовы LevelEditSession. Раньше это было 60 строк прямо в ViewportPanel::update.
//  Сама сессия (level/) остаётся без ImGui: она получает ViewportPointer и LevelKey.
//

#pragma once

#include "ViewportImageRect.h"
#include "Input.h"

#include "level/ViewportPointer.h"

#include <glm/glm.hpp>

class EditorViewport;
class LevelEditSession;

namespace LevelEditInput
{
    // Указатель мыши для инструментов правки в координатах вида.
    //   mouse       позиция курсора в экранных координатах ImGui
    //   hoverInput  панель вправе принять ввод от наведения (никто не держит мышь)
    ViewportPointer MakePointer(EditorViewport& viewport, const ViewportImageRect& image,
                                const glm::vec2& mouse, bool hoverInput,
                                const Input::State& physicalInput);

    struct ShortcutResult
    {
        bool saveRequested = false;   // Ctrl / Cmd + S: сохранение уровня за вызывающим (сессия о карте не знает)
    };

    // Горячие клавиши правки. Вызывать, когда панель в фокусе и камера не летит.
    // Сама проверяет, что текст не вводится в другое поле.
    ShortcutResult HandleShortcuts(LevelEditSession& edit);
}
