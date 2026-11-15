//
//  ViewportPicking.h
//  Cucumber
//
//  Пикинг мировой позиции через чтение буфера глубины вьюпорта. Вынесено из ViewportPanel.cpp без изменений
//  поведения: чистые функции от камеры, цели рендера и позиции мыши.
//

#pragma once

#include <glm/glm.hpp>

class EditorCamera;
class RenderTarget;

namespace ViewportPicking
{
    // Точка поверхности под курсором и её нормаль (оценивается по соседним пикселям глубины;
    // если оценить не удалось, normal нулевая, но результат всё равно true). false: под курсором небо.
    bool SurfaceAt(const EditorCamera& camera, const RenderTarget& target,
                   const glm::ivec2& viewportSize, const glm::vec2& mousePosition,
                   glm::vec3& position, glm::vec3& normal);

    // Куда ставить новую сущность: поверхность под курсором, иначе точка на луче в 64 юнитах от камеры
    glm::vec3 CreationPosition(const EditorCamera& camera, const RenderTarget& target,
                               const glm::ivec2& viewportSize, const glm::vec2& mousePosition);
}
