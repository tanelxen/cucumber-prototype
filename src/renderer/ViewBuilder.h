//
//  ViewBuilder.h
//  Cucumber
//

#pragma once

#include "RenderScene.h"
#include "ViewDesc.h"

#include <vector>

class ShadowSystem;

// Для каждого ViewDesc: выбрать свет (со слотами теней), солнце вида, отсечь объекты, собрать команды
// и положить RenderView в RenderDevice. Вызывать после ShadowSystem::plan().
void SubmitViews(const RenderScene& scene, const std::vector<ViewDesc>& views, ShadowSystem& shadows);
