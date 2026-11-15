//
//  Brush.h
//  Cucumber
//
//  Построение выпуклых тел.
//

#pragma once

#include "Polygon.h"

#include <cstdint>
#include <vector>

namespace Geo
{
    // Выпуклое тело как пересечение полупространств (нормали наружу, "внутри" = signedDistance <= 0).
    // Классический способ: на каждой плоскости берётся огромный полигон и отсекается всеми остальными.
    // Устойчивее, чем пересечение троек плоскостей с эпсилон-фильтрацией.
    //
    // out[i] — полигон грани i (против часовой снаружи) или пустой, если грань вырождена, продублирована
    // или целиком лежит вне тела. Возвращает true, если получилось хотя бы 4 непустые грани.
    bool BuildBrushPolygons(const std::vector<Plane>& planes, std::vector<Polygon>& out);

    // Выпуклая оболочка набора точек (для правки вершин). Перебирает опорные плоскости по тройкам точек,
    // поэтому рассчитана на небольшие тела (десятки вершин, не более 128).
    // false, если точки не образуют объём.
    bool ConvexHullPolygons(const std::vector<Vec3>& points, std::vector<Polygon>& out);

    // Превращает набор полигонов в индексированное представление: общие вершины сливаются в пределах weldEps.
    // loops[i] соответствует polygons[i]; после слияния петля может выродиться (< 3 вершин) и станет пустой.
    void IndexPolygons(const std::vector<Polygon>& polygons, double weldEps,
                       std::vector<Vec3>& vertices, std::vector<std::vector<uint32_t>>& loops);
}
