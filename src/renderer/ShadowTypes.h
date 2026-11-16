//
//  ShadowTypes.h
//  Cucumber
//
//  Типы, которые нужны и RenderView, и ShadowSystem (чтобы RenderDevice.h не включал ShadowSystem.h).
//

#pragma once

#include <vector>
#include <glm/glm.hpp>

struct RenderObject;

// Солнечная тень для одного вида. Единственная тень, зависящая от камеры.
struct SunShadow
{
    bool enabled = false;   // у вида есть тень солнца (слот 0 в массиве 2D-карт)
    bool render = false;    // карту надо перерисовать для этого вида (базовые виды); оверлеи используют готовую
    glm::mat4 matrix{1.0f};
    std::vector<const RenderObject*> casters;
};
