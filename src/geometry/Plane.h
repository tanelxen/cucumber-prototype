//
//  Plane.h
//  Cucumber
//

#pragma once

#include <cmath>
#include <glm/glm.hpp>

namespace Geo
{
    using Vec3 = glm::dvec3;

    // Плоскость dot(normal, p) == distance. Нормаль смотрит НАРУЖУ тела.
    struct Plane
    {
        Vec3 normal{0.0, 1.0, 0.0};
        double distance = 0.0;

        double signedDistance(const Vec3& p) const { return glm::dot(normal, p) - distance; }

        Plane flipped() const { return { -normal, -distance }; }

        // Нормаль = normalize(cross(b - a, c - a)). false, если точки почти на одной прямой.
        static bool FromPoints(const Vec3& a, const Vec3& b, const Vec3& c, Plane& out)
        {
            const Vec3 n = glm::cross(b - a, c - a);
            const double length = glm::length(n);
            if (length < 1e-12) return false;

            out.normal = n / length;
            out.distance = glm::dot(out.normal, a);
            return true;
        }
    };

    inline bool SamePlane(const Plane& a, const Plane& b, double eps)
    {
        return glm::dot(a.normal, b.normal) > 1.0 - 1e-9 && std::abs(a.distance - b.distance) <= eps;
    }
}
