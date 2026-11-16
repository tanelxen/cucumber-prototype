//
//  SceneExtractor.h
//  Cucumber
//

#pragma once

//#include "renderer/RenderScene.h"

class WorldScene;
class RenderScene;

// Единственное место, которое читает компоненты ради рендера.
// Сцена про рендер ничего не знает, рендер сцену не меняет.
void ExtractRenderScene(WorldScene& world, RenderScene& out);
