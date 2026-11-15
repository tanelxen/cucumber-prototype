//
//  ViewportImageRect.h
//  Cucumber
//
//  Прямоугольник изображения вьюпорта в экранных координатах ImGui (пиксели).
//  Заменяет четыре поля m_imageLeft/Top/Right/Bottom плюс флаг и повторяющиеся выражения
//  std::max(1, int(right - left)) в панели.
//

#pragma once

#include <algorithm>

#include <glm/glm.hpp>

struct ViewportImageRect
{
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    bool valid = false;      // изображение нарисовано и имеет ненулевой размер

    static ViewportImageRect FromMinMax(float minX, float minY, float maxX, float maxY)
    {
        ViewportImageRect rect;
        rect.left = minX;
        rect.top = minY;
        rect.right = maxX;
        rect.bottom = maxY;
        rect.valid = maxX > minX && maxY > minY;
        return rect;
    }

    // Размер в пикселях, не меньше 1x1 (безопасен как делитель)
    glm::ivec2 size() const
    {
        return { std::max(1, static_cast<int>(right - left)),
                 std::max(1, static_cast<int>(bottom - top)) };
    }

    bool contains(float screenX, float screenY) const
    {
        return valid && screenX >= left && screenX < right && screenY >= top && screenY < bottom;
    }

    glm::vec2 toLocal(float screenX, float screenY) const
    {
        return { screenX - left, screenY - top };
    }

    // Прижимает локальную точку к границам изображения
    glm::vec2 clampLocal(const glm::vec2& local) const
    {
        const glm::ivec2 s = size();
        return { std::clamp(local.x, 0.0f, static_cast<float>(s.x)),
                 std::clamp(local.y, 0.0f, static_cast<float>(s.y)) };
    }
};
