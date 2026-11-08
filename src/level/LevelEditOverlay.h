//
//  LevelEditOverlay.h
//  Cucumber
//
//  Оверлеи режима правки геометрии.
//   - DrawLevelEditOverlay: ... Вызывается раз в кадр из UIManager::collectViews, до рендера видов
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
