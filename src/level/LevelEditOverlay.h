//
//  LevelEditOverlay.h
//  Cucumber
//
//  Подсветка выделения и наведения в режиме правки геометрии. Рисует через GizmoRender, поэтому вызывать
//  надо раз в кадр ДО рендера видов (очередь GizmoRender общая для всех видов и чистится после рендера).
//

#pragma once

class LevelEditSession;

void DrawLevelEditOverlay(const LevelEditSession& session);
