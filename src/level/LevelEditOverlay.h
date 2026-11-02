//
//  LevelEditOverlay.h
//  Cucumber
//
//  Оверлеи режима правки геометрии.
//   - DrawLevelEditOverlay: линии (выделение, наведение, превью активного инструмента) через GizmoRender.
//     Вызывать раз в кадр ДО рендера видов: очередь GizmoRender общая для всех видов и чистится после рендера.
//   - DrawLevelEditLabels: подписи превью (размеры бокса, смещение грани) в экранных координатах ImGui.
//     DrawText3D для этого не годится: он строит билборды по камере последнего flush, а видов несколько.
//     Вызывать из ViewportPanel::draw() после ImGui::Image, пока текущее окно это окно вьюпорта.
//

#pragma once

#include "ViewDesc.h"

class LevelEditSession;

void DrawLevelEditOverlay(const LevelEditSession& session);

// x, y, width, height: прямоугольник картинки вида в экранных координатах ImGui
void DrawLevelEditLabels(const LevelEditSession& session, const CameraData& camera, float x, float y, float width, float height);
