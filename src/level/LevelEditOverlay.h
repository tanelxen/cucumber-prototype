//
//  LevelEditOverlay.h
//  Cucumber
//
//  Оверлеи режима правки геометрии.
//   - DrawLevelEditOverlay: линии и маркеры (выбранные солиды и грани, превью и ручки активного инструмента) через
//     OverlayRender: GPU, толщина и сглаживание, рисуется в цель вида. Вызывать раз в кадр ДО рендера видов
//     (накопленное одно на все виды и чистится после рендера, см. OverlayRender::Clear).
//   - DrawLevelEditLabels: только подписи превью (размеры бокса, смещение грани) в экранных координатах ImGui:
//     текст нужен читаемым любого размера, а шрифта в GPU-оверлее нет. Вызывать из ViewportPanel::draw() после
//     ImGui::Image, пока текущее окно это окно вьюпорта.
//

#pragma once

#include "ViewDesc.h"

class LevelEditSession;

void DrawLevelEditOverlay(const LevelEditSession& session);

// x, y, width, height: прямоугольник картинки вида в экранных координатах ImGui
void DrawLevelEditLabels(const LevelEditSession& session, const CameraData& camera, float x, float y, float width, float height);
