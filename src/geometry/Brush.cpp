//
//  Brush.cpp
//  Cucumber
//

#include "Brush.h"

#include "Epsilon.h"

#include <algorithm>

namespace Geo
{
    bool BuildBrushPolygons(const std::vector<Plane>& planes, std::vector<Polygon>& out)
    {
        out.assign(planes.size(), Polygon{});

        int valid = 0;
        for (size_t i = 0; i < planes.size(); ++i)
        {
            // Продублированная плоскость даёт ту же грань: оставляем первую
            bool duplicate = false;
            for (size_t j = 0; j < i && !duplicate; ++j) duplicate = SamePlane(planes[i], planes[j], kOnPlane);
            if (duplicate) continue;

            Polygon polygon = BasePolygonForPlane(planes[i], kWorldExtent);

            for (size_t j = 0; j < planes.size() && !polygon.empty(); ++j)
            {
                if (j == i) continue;

                Polygon front, back;
                ClipPolygon(polygon, planes[j], kOnPlane, front, back);
                polygon = std::move(back);   // остаётся часть "внутри" плоскости j
            }

            RemoveDuplicateVertices(polygon, kWeld);
            if (polygon.size() < 3 || Area(polygon) < kMinArea) continue;

            out[i] = std::move(polygon);
            ++valid;
        }

        return valid >= 4;
    }

    bool ConvexHullPolygons(const std::vector<Vec3>& input, std::vector<Polygon>& out)
    {
        out.clear();

        // Слияние близких точек
        std::vector<Vec3> points;
        for (const Vec3& p : input)
        {
            const bool known = std::any_of(points.begin(), points.end(),
                                           [&](const Vec3& q) { return glm::distance(p, q) <= kWeld; });
            if (!known) points.push_back(p);
        }
        if (points.size() < 4 || points.size() > 128) return false;

        // Опорные плоскости: через каждую тройку точек, если все остальные лежат по одну сторону
        std::vector<Plane> planes;
        for (size_t i = 0; i < points.size(); ++i)
        for (size_t j = i + 1; j < points.size(); ++j)
        for (size_t k = j + 1; k < points.size(); ++k)
        {
            Plane plane;
            if (!Plane::FromPoints(points[i], points[j], points[k], plane)) continue;

            bool anyFront = false;
            bool anyBack = false;
            for (const Vec3& p : points)
            {
                const double d = plane.signedDistance(p);
                anyFront = anyFront || d > kOnPlane;
                anyBack = anyBack || d < -kOnPlane;
                if (anyFront && anyBack) break;
            }
            if (anyFront && anyBack) continue;   // точки по обе стороны: не опорная
            if (!anyFront && !anyBack) continue; // все точки в одной плоскости: объёма нет

            if (anyFront) plane = plane.flipped();   // нормаль наружу: все точки позади

            const bool duplicate = std::any_of(planes.begin(), planes.end(),
                                               [&](const Plane& q) { return SamePlane(plane, q, kOnPlane); });
            if (!duplicate) planes.push_back(plane);
        }

        if (planes.size() < 4) return false;
        return BuildBrushPolygons(planes, out);
    }

    void IndexPolygons(const std::vector<Polygon>& polygons, double weldEps,
                       std::vector<Vec3>& vertices, std::vector<std::vector<uint32_t>>& loops)
    {
        vertices.clear();
        loops.assign(polygons.size(), std::vector<uint32_t>{});

        for (size_t i = 0; i < polygons.size(); ++i)
        {
            std::vector<uint32_t>& loop = loops[i];

            for (const Vec3& p : polygons[i])
            {
                uint32_t index = static_cast<uint32_t>(vertices.size());
                for (size_t v = 0; v < vertices.size(); ++v)
                {
                    if (glm::distance(vertices[v], p) <= weldEps) { index = static_cast<uint32_t>(v); break; }
                }
                if (index == vertices.size()) vertices.push_back(p);

                if (loop.empty() || loop.back() != index) loop.push_back(index);
            }
            while (loop.size() > 1 && loop.front() == loop.back()) loop.pop_back();

            // После слияния в петле могли остаться повторы не подряд: такая грань вырождена
            std::vector<uint32_t> sorted = loop;
            std::sort(sorted.begin(), sorted.end());
            if (loop.size() < 3 || std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end()) loop.clear();
        }
    }
}
