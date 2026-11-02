//
//  EditMath.cpp
//  Cucumber
//

#include "EditMath.h"

#include <algorithm>
#include <cmath>

namespace Edit
{
    namespace
    {
        constexpr double kMinSin2 = 0.01;   // sin^2 минимального угла между лучом и прямой (около 5.7 градусов)
        constexpr double kEps = 1e-12;
    }

    bool RayPlane(const Ray& ray, const glm::dvec3& planePoint, const glm::dvec3& planeNormal, double& outT)
    {
        const double denominator = glm::dot(planeNormal, ray.dir);
        if (std::abs(denominator) < 1e-6) return false;

        const double t = glm::dot(planeNormal, planePoint - ray.origin) / denominator;
        if (t < 0.0 || t > ray.maxDistance) return false;

        outT = t;
        return true;
    }

    bool ClosestLineParam(const Ray& ray, const glm::dvec3& linePoint, const glm::dvec3& lineDir, double& outT)
    {
        const glm::dvec3 w = linePoint - ray.origin;
        const double b = glm::dot(lineDir, ray.dir);
        const double denominator = 1.0 - b * b;
        if (denominator < kMinSin2) return false;

        outT = (b * glm::dot(ray.dir, w) - glm::dot(lineDir, w)) / denominator;
        return true;
    }

    SegmentApproach ClosestRaySegment(const Ray& ray, const glm::dvec3& a, const glm::dvec3& b)
    {
        // Расстояние между двумя отрезками (Ericson, Real-Time Collision Detection); луч это отрезок длиной maxDistance
        const glm::dvec3 d1 = ray.dir * ray.maxDistance;
        const glm::dvec3 d2 = b - a;
        const glm::dvec3 r = ray.origin - a;

        const double a1 = glm::dot(d1, d1);
        const double e = glm::dot(d2, d2);
        const double f = glm::dot(d2, r);
        const double c = glm::dot(d1, r);

        double s = 0.0;
        double t = 0.0;

        if (e <= kEps)
        {
            s = std::clamp(-c / a1, 0.0, 1.0);
        }
        else
        {
            const double bb = glm::dot(d1, d2);
            const double denominator = a1 * e - bb * bb;
            s = denominator > kEps ? std::clamp((bb * f - c * e) / denominator, 0.0, 1.0) : 0.0;
            t = (bb * s + f) / e;

            if (t < 0.0)      { t = 0.0; s = std::clamp(-c / a1, 0.0, 1.0); }
            else if (t > 1.0) { t = 1.0; s = std::clamp((bb - c) / a1, 0.0, 1.0); }
        }

        SegmentApproach result;
        result.rayPoint = ray.origin + d1 * s;
        result.segmentPoint = a + d2 * t;
        result.rayT = s * ray.maxDistance;
        result.distance = glm::length(result.rayPoint - result.segmentPoint);
        return result;
    }

    double Snap(double value, double grid)
    {
        if (grid <= 0.0) return value;
        return std::round(value / grid) * grid;
    }

    glm::dvec3 SnapVec(const glm::dvec3& v, double grid)
    {
        return { Snap(v.x, grid), Snap(v.y, grid), Snap(v.z, grid) };
    }

    int DominantAxis(const glm::dvec3& v)
    {
        const glm::dvec3 a = glm::abs(v);
        if (a.x >= a.y && a.x >= a.z) return 0;
        return a.y >= a.z ? 1 : 2;
    }

    glm::dvec3 AxisNormal(const glm::dvec3& v)
    {
        const int axis = DominantAxis(v);
        glm::dvec3 n(0.0);
        n[axis] = v[axis] < 0.0 ? -1.0 : 1.0;
        return n;
    }
}
