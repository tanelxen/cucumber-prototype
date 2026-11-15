//
//  EditorWidgets.h
//  Cucumber
//
//  Общие виджеты редактора на ImGui.
//

#pragma once

namespace EditorWidgets
{
    // Кнопки Play / Stop 32x32 с нарисованной иконкой. true в кадре нажатия.
    bool PlayButton(const char* id = "##ViewportPlay");
    bool StopButton(const char* id = "##ViewportStop");
}
