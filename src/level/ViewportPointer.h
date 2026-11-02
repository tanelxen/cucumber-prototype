//
//  ViewportPointer.h
//  Cucumber
//
//  Состояние указателя над одним видом за кадр. Заполняет панель вьюпорта, читают инструменты.
//

#pragma once

#include "EditMath.h"
#include "ViewDesc.h"

#include <algorithm>

struct ViewportPointer
{
    CameraData camera;
    glm::ivec2 sizePx{1, 1};
    glm::vec2 mousePx{0.0f};     // относительно левого верхнего угла вида (может выходить за границы вида)
    bool over = false;           // курсор над видом и вид вправе принять ввод (никто не держит мышь)
    bool capturing = false;      // камера летит (ПКМ): курсор скрыт, mousePx не имеет смысла, инструменты замирают
    bool pressed = false;        // ЛКМ нажата именно в этом кадре
    bool down = false;
    bool released = false;       // ЛКМ отпущена именно в этом кадре
    bool ctrl = false;           // Ctrl или Cmd
    bool shift = false;
    bool alt = false;
};

inline Edit::Ray MakeRay(const ViewportPointer& pointer)
{
    glm::vec3 origin, direction;
    ScreenPointToRay(pointer.camera, pointer.sizePx, pointer.mousePx, origin, direction);

    Edit::Ray ray;
    ray.origin = glm::dvec3(origin);
    ray.dir = glm::normalize(glm::dvec3(direction));
    // В ортографии луч стартует на ближней плоскости (нижняя граница диапазона), в перспективе в глазу
    ray.maxDistance = static_cast<double>(pointer.camera.farZ - pointer.camera.nearZ) * 1.01;
    return ray;
}

// Допуск "курсор рядом" в пикселях экрана, пересчитанный в мировые единицы
inline Edit::PickTolerance PixelTolerance(const ViewportPointer& pointer, float pixels)
{
    const double p11 = std::max(1e-6f, std::abs(pointer.camera.proj[1][1]));
    const double perPixel = 2.0 / (p11 * std::max(1, pointer.sizePx.y));   // мировых единиц на пиксель: глубина 1 (persp) или всегда (ortho)

    Edit::PickTolerance tolerance;
    if (pointer.camera.ortho) tolerance.base = perPixel * pixels;
    else                      tolerance.perDistance = perPixel * pixels;
    return tolerance;
}
