//
//  Polygon.h
//  Cucumber
//
//  Операции над выпуклыми и плоскими полигонами. Порядок вершин: против часовой стрелки, если смотреть
//  со стороны нормали (снаружи тела). Все операции этот порядок сохраняют.
//

#pragma once

#include "Plane.h"

#include <cmath>
#include <vector>

namespace Geo
{
    using Polygon = std::vector<Vec3>;

    // Нормаль по методу Ньюэлла, НЕ нормализована: длина равна удвоенной площади.
    // Работает для любых плоских полигонов, не зависит от выбора начальной вершины.
    Vec3 NewellNormal(const Polygon& polygon);

    double Area(const Polygon& polygon);
    Vec3 Centroid(const Polygon& polygon);   // среднее арифметическое вершин (для выпуклого полигона лежит внутри)

    // Все вершины в пределах eps от плоскости Ньюэлла
    bool IsPlanar(const Polygon& polygon, double eps);

    // Огромный квадрат на плоскости, вершины против часовой стрелки вокруг нормали
    Polygon BasePolygonForPlane(const Plane& plane, double extent);

    // Рассекает полигон плоскостью. front — часть по стороне нормали, back — с противоположной.
    // Точки в пределах eps от плоскости попадают в обе части. Если полигон целиком по одну сторону,
    // вторая часть пуста, а первая равна исходному полигону.
    void ClipPolygon(const Polygon& polygon, const Plane& plane, double eps, Polygon& front, Polygon& back);

    // Убирает подряд идущие (включая последнюю и первую) вершины ближе eps
    void RemoveDuplicateVertices(Polygon& polygon, double eps);

    // Веер из первой вершины. Корректен только для ВЫПУКЛЫХ полигонов. Индексы по loop.size().
    void TriangulateConvex(size_t vertexCount, std::vector<unsigned>& outIndices, unsigned baseIndex = 0);
}
