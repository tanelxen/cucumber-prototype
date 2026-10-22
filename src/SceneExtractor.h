//
//  SceneExtractor.h
//  Cucumber
//

#pragma once

#include "RenderScene.h"

class WorldScene;

// Единственное место, которое читает компоненты ради рендера.
// Сцена про рендер ничего не знает, рендер сцену не меняет.
void ExtractRenderScene(WorldScene& world, RenderScene& out);
