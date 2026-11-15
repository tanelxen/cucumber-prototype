//
//  ViewportToolbar.h
//  Cucumber
//
//  Тулбар панели вьюпорта: Grid / Gizmos, режим вида, затенение, шаг сетки, Play / Stop.
//  Только рисует и меняет настройки вида; что делать с Play / Stop, решает панель.
//

#pragma once

class EditorViewport;

enum class ToolbarAction { None, Play, Stop };

// Вызывать сразу после ImGui::Begin панели.
//   gridSize      шаг сетки документа (меняется комбо и клавишами [ / ])
//   showGridSize  показывать ли выбор шага (раньше только когда в сцене есть карта)
//   focused       окно в фокусе: только тогда обрабатываются [ и ], иначе при нескольких панелях сработали бы дважды
ToolbarAction DrawViewportToolbar(EditorViewport& viewport, float& gridSize,
                                  bool showGridSize, bool focused, bool simulating);
