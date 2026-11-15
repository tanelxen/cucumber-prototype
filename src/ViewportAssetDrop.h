//
//  ViewportAssetDrop.h
//  Cucumber
//
//  Drag-and-drop мешей и префабов из панели ассетов на вьюпорт.
//

#pragma once

#include "ViewportImageRect.h"

#include <functional>

#include <glm/glm.hpp>

class WorldScene;
class Entity;

namespace ViewportAssetDrop
{
    // Вызывать СРАЗУ после ImGui::Image вьюпорта: цель drag-and-drop привязывается к последнему элементу.
    //   positionAt  локальная позиция в изображении -> мировая точка создания
    //   gridStep    шаг привязки позиции к сетке
    // Возвращает созданную сущность в кадре отпускания, иначе nullptr. Выбор сущности в UI остаётся за вызывающим.
    Entity* Handle(WorldScene& scene, const ViewportImageRect& image,
                   const std::function<glm::vec3(const glm::vec2&)>& positionAt, float gridStep);
}
