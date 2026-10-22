//
//  Bounds.h
//  Cucumber
//
//  AABB и Frustum. Нужны везде, где появляется отсечение: камеры, тени от света, запросы к карте.
//

#pragma once

#include <cfloat>
#include <glm/glm.hpp>

struct AABB
{
    glm::vec3 min{ FLT_MAX };
    glm::vec3 max{ -FLT_MAX };

    bool valid() const { return min.x <= max.x; }
    glm::vec3 center() const { return (min + max) * 0.5f; }

    void expand(const glm::vec3& p) { min = glm::min(min, p); max = glm::max(max, p); }
    void expand(const AABB& b)      { min = glm::min(min, b.min); max = glm::max(max, b.max); }

    bool intersects(const AABB& o) const
    {
        return min.x <= o.max.x && max.x >= o.min.x &&
               min.y <= o.max.y && max.y >= o.min.y &&
               min.z <= o.max.z && max.z >= o.min.z;
    }

    // Для world-space bounds динамических объектов (8 углов).
    AABB transformed(const glm::mat4& m) const
    {
        AABB r;
        for (int i = 0; i < 8; ++i)
        {
            const glm::vec3 corner((i & 1) ? max.x : min.x, (i & 2) ? max.y : min.y, (i & 4) ? max.z : min.z);
            r.expand(glm::vec3(m * glm::vec4(corner, 1.0f)));
        }
        return r;
    }
};

struct Frustum
{
    glm::vec4 planes[6]; // left, right, bottom, top, near, far; нормали смотрят внутрь

    Frustum() = default;

    // viewProj = proj * view (OpenGL clip space). Работает и для перспективы, и для ортографии.
    explicit Frustum(const glm::mat4& m)
    {
        const glm::vec4 r0(m[0][0], m[1][0], m[2][0], m[3][0]);
        const glm::vec4 r1(m[0][1], m[1][1], m[2][1], m[3][1]);
        const glm::vec4 r2(m[0][2], m[1][2], m[2][2], m[3][2]);
        const glm::vec4 r3(m[0][3], m[1][3], m[2][3], m[3][3]);

        planes[0] = r3 + r0;
        planes[1] = r3 - r0;
        planes[2] = r3 + r1;
        planes[3] = r3 - r1;
        planes[4] = r3 + r2;
        planes[5] = r3 - r2;

        for (auto& p : planes)
        {
            const float len = glm::length(glm::vec3(p));
            if (len > 0.0f) p /= len;
        }
    }

    bool intersects(const AABB& b) const
    {
        for (const auto& p : planes)
        {
            // Вершина бокса, максимально далёкая "внутрь" плоскости
            const glm::vec3 v(p.x >= 0.0f ? b.max.x : b.min.x,
                              p.y >= 0.0f ? b.max.y : b.min.y,
                              p.z >= 0.0f ? b.max.z : b.min.z);
            if (glm::dot(glm::vec3(p), v) + p.w < 0.0f) return false;
        }
        return true;
    }
};
