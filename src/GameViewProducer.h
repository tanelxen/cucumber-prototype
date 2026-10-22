//
//  GameViewProducer.h
//  Cucumber
//
//  Превращает стек игровых камер в список ViewDesc.
//  Не знает ни про панели, ни про сцену — только про CameraManager и цель, куда рисовать.
//

#pragma once

#include "ViewDesc.h"

#include <vector>

class CameraManager;
class RenderTarget;

struct GameViewParams
{
    RenderTarget* target = nullptr;   // куда рисуют камеры без собственной цели
    glm::ivec2 size{1, 1};            // логический размер этой цели
    uint32_t stackId = 0;
    uint32_t baseViewId = 0;

    bool drawSkybox = true;
    bool drawGameUI = true;
    bool audioListener = true;
};

// Возвращает false, если в стеке нет живой базовой камеры (вызывающий решает, что рисовать вместо).
bool ProduceGameViews(const CameraManager& cameras, const GameViewParams& params, std::vector<ViewDesc>& out);
