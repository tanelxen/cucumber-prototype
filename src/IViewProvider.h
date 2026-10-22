//
//  IViewProvider.h
//  Cucumber
//
//  Кто-то, кто знает, ЧТО показывать: редактор (панели вьюпортов) или игра (стек камер).
//  Renderer про UI ничего не знает и спрашивает только этот интерфейс.
//

#pragma once

#include "ViewDesc.h"

#include <vector>

class WorldScene;

class IViewProvider
{
public:
    virtual ~IViewProvider() = default;

    // Добавляет в out описания видов для этого кадра.
    virtual void collectViews(WorldScene& scene, std::vector<ViewDesc>& out) = 0;
};
