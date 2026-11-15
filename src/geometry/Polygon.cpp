//
//  Polygon.cpp
//  Cucumber
//

#include "Polygon.h"

#include <algorithm>

namespace Geo
{
    Vec3 NewellNormal(const Polygon& polygon)
    {
        Vec3 normal(0.0);
        const size_t count = polygon.size();
        for (size_t i = 0; i < count; ++i)
        {
            const Vec3& a = polygon[i];
            const Vec3& b = polygon[(i + 1) % count];
            normal.x += (a.y - b.y) * (a.z + b.z);
            normal.y += (a.z - b.z) * (a.x + b.x);
            normal.z += (a.x - b.x) * (a.y + b.y);
        }
        return normal;
    }

    double Area(const Polygon& polygon)
    {
        return 0.5 * glm::length(NewellNormal(polygon));
    }

    Vec3 Centroid(const Polygon& polygon)
    {
        Vec3 sum(0.0);
        for (const Vec3& p : polygon) sum += p;
        return polygon.empty() ? sum : sum / static_cast<double>(polygon.size());
    }

    bool IsPlanar(const Polygon& polygon, double eps)
    {
        if (polygon.size() < 3) return false;

        const Vec3 n = NewellNormal(polygon);
        const double length = glm::length(n);
        if (length < 1e-12) return false;

        const Vec3 normal = n / length;
        const double distance = glm::dot(normal, Centroid(polygon));
        for (const Vec3& p : polygon)
        {
            if (std::abs(glm::dot(normal, p) - distance) > eps) return false;
        }
        return true;
    }

    Polygon BasePolygonForPlane(const Plane& plane, double extent)
    {
        // Ось, наименее совпадающая с нормалью, даёт устойчивый "вверх"
        const Vec3 n = plane.normal;
        Vec3 axis(0.0, 0.0, 1.0);
        const Vec3 absN = glm::abs(n);
        if (absN.x <= absN.y && absN.x <= absN.z)      axis = Vec3(1.0, 0.0, 0.0);
        else if (absN.y <= absN.x && absN.y <= absN.z) axis = Vec3(0.0, 1.0, 0.0);

        Vec3 up = glm::normalize(axis - n * glm::dot(axis, n));
        Vec3 right = glm::cross(up, n);

        up *= extent;
        right *= extent;
        const Vec3 origin = n * plane.distance;

        Polygon polygon = {
            origin - right + up,
            origin + right + up,
            origin + right - up,
            origin - right - up,
        };

        // Гарантируем порядок против часовой стрелки вокруг нормали
        if (glm::dot(NewellNormal(polygon), n) < 0.0) std::reverse(polygon.begin(), polygon.end());
        return polygon;
    }

    void ClipPolygon(const Polygon& polygon, const Plane& plane, double eps, Polygon& front, Polygon& back)
    {
        front.clear();
        back.clear();

        const size_t count = polygon.size();
        if (count == 0) return;

        std::vector<double> distances(count + 1);
        std::vector<int> sides(count + 1);
        int frontCount = 0;
        int backCount = 0;

        for (size_t i = 0; i < count; ++i)
        {
            distances[i] = plane.signedDistance(polygon[i]);
            if (distances[i] > eps)        { sides[i] = 1;  ++frontCount; }
            else if (distances[i] < -eps)  { sides[i] = -1; ++backCount; }
            else                           { sides[i] = 0; }
        }
        distances[count] = distances[0];
        sides[count] = sides[0];

        if (frontCount == 0) { back = polygon; return; }
        if (backCount == 0)  { front = polygon; return; }

        for (size_t i = 0; i < count; ++i)
        {
            const Vec3& p1 = polygon[i];

            if (sides[i] == 0)
            {
                front.push_back(p1);
                back.push_back(p1);
                continue;
            }

            (sides[i] > 0 ? front : back).push_back(p1);

            if (sides[i + 1] == 0 || sides[i + 1] == sides[i]) continue;

            const Vec3& p2 = polygon[(i + 1) % count];
            const double t = distances[i] / (distances[i] - distances[i + 1]);
            const Vec3 mid = p1 + (p2 - p1) * t;
            front.push_back(mid);
            back.push_back(mid);
        }
    }

    void RemoveDuplicateVertices(Polygon& polygon, double eps)
    {
        if (polygon.size() < 2) return;

        Polygon result;
        result.reserve(polygon.size());
        for (const Vec3& p : polygon)
        {
            if (result.empty() || glm::distance(result.back(), p) > eps) result.push_back(p);
        }
        while (result.size() > 1 && glm::distance(result.front(), result.back()) <= eps) result.pop_back();
        polygon = std::move(result);
    }

    void TriangulateConvex(size_t vertexCount, std::vector<unsigned>& outIndices, unsigned baseIndex)
    {
        for (size_t i = 1; i + 1 < vertexCount; ++i)
        {
            outIndices.push_back(baseIndex);
            outIndices.push_back(baseIndex + static_cast<unsigned>(i));
            outIndices.push_back(baseIndex + static_cast<unsigned>(i) + 1);
        }
    }
}
