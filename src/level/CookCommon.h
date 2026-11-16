//
//  CookCommon.h
//  Cucumber
//
//  То, что обязано совпадать у кукера (LevelCooker, режим игры) и у редактируемой геометрии (EditGeometry, режим
//  правки): размер ячейки нарезки, порядок обхода граней, формула UV. Одно определение на двоих, чтобы уровень в
//  режиме правки выглядел ровно так же, как после выхода из него.
//

#pragma once

#include "level/LevelDocument.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/glm.hpp>

namespace CookCommon
{
    constexpr float kChunkSize = 1024.0f;          // размер ячейки нарезки

    // Порядок обхода граней в готовом меше. Прежний билдер писал треугольники ПО часовой стрелке (если смотреть
    // снаружи), и весь рендер карты (reverseWinding = true в SceneExtractor, cull GL_FRONT) на это рассчитан.
    // Когда прежний билдер будет удалён: поставить false и убрать reverseWinding у объектов карты в SceneExtractor.
    constexpr bool kEmitLegacyWinding = false;

    struct Cell
    {
        int x = 0, y = 0, z = 0;
    };

    // Ячейка определяется по центру ВСЕГО солида (включая NoDraw-грани): грани солида идут в одну ячейку
    inline Cell CellOfSolid(const Solid& solid)
    {
        const glm::vec3 cell = SolidBounds(solid).center() / kChunkSize;
        return { static_cast<int>(std::floor(cell.x)), static_cast<int>(std::floor(cell.y)), static_cast<int>(std::floor(cell.z)) };
    }

    // Вершины грани в порядке выдачи треугольников (с учётом порядка обхода)
    inline std::vector<glm::vec3> FacePoints(const Solid& solid, const Face& face)
    {
        std::vector<glm::vec3> points;
        points.reserve(face.loop.size());
        for (uint32_t index : face.loop) points.push_back(solid.vertices[index]);
        if (kEmitLegacyWinding) std::reverse(points.begin(), points.end());
        return points;
    }

    inline glm::vec2 ValveUV(const glm::vec3& pos, const FaceUV& uv, int texWidth, int texHeight)
    {
        // защита от нулевого масштаба (TrenchBroom трактует 0 как 1)
        const float sx = uv.scale.x != 0.0f ? uv.scale.x : 1.0f;
        const float sy = uv.scale.y != 0.0f ? uv.scale.y : 1.0f;

        const float u = glm::dot(pos, uv.uAxis) / sx + uv.shift.x;
        const float v = glm::dot(pos, uv.vAxis) / sy + uv.shift.y;
        return { u / static_cast<float>(texWidth), v / static_cast<float>(texHeight) };
    }
}
