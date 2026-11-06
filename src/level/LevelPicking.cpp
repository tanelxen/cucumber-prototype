//
//  LevelPicking.cpp
//  Cucumber
//

#include "LevelPicking.h"

#include "geometry/Polygon.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

namespace
{
    // Пересечение луча с AABB (slab-метод). Нужен как быстрый отсев тел до проверки граней.
    bool RayHitsBounds(const AABB& box, const glm::dvec3& origin, const glm::dvec3& direction, double maxDistance)
    {
        double tMin = 0.0;
        double tMax = maxDistance;

        for (int axis = 0; axis < 3; ++axis)
        {
            const double lo = box.min[axis] - 0.01;
            const double hi = box.max[axis] + 0.01;

            if (std::abs(direction[axis]) < 1e-12)
            {
                if (origin[axis] < lo || origin[axis] > hi) return false;
                continue;
            }

            double t0 = (lo - origin[axis]) / direction[axis];
            double t1 = (hi - origin[axis]) / direction[axis];
            if (t0 > t1) std::swap(t0, t1);

            tMin = std::max(tMin, t0);
            tMax = std::min(tMax, t1);
            if (tMin > tMax) return false;
        }
        return true;
    }

    bool RaycastSolidD(const Solid& solid, const glm::dvec3& origin, const glm::dvec3& direction, double maxDistance,
                       double& outDistance, size_t& outFace)
    {
        // Выпуклое тело = пересечение полупространств. Ищем отрезок луча [tEnter, tExit] внутри всех.
        double tEnter = 0.0;
        double tExit = maxDistance;
        size_t enterFace = static_cast<size_t>(-1);

        for (size_t f = 0; f < solid.faces.size(); ++f)
        {
            const Face& face = solid.faces[f];

            Geo::Polygon polygon;
            polygon.reserve(face.loop.size());
            for (uint32_t index : face.loop) polygon.push_back(Geo::Vec3(solid.vertices[index]));

            const Geo::Vec3 n = Geo::NewellNormal(polygon);
            const double length = glm::length(n);
            if (length < 1e-12) continue;

            const glm::dvec3 normal = n / length;
            const double planeDistance = glm::dot(normal, Geo::Centroid(polygon));

            const double distanceToPlane = glm::dot(normal, origin) - planeDistance;   // > 0: origin перед плоскостью
            const double slope = glm::dot(normal, direction);

            if (std::abs(slope) < 1e-12)
            {
                if (distanceToPlane > 0.0) return false;   // параллельно и снаружи
                continue;
            }

            const double t = -distanceToPlane / slope;
            if (slope < 0.0)   // луч входит через эту грань
            {
                if (t > tEnter) { tEnter = t; enterFace = f; }
            }
            else               // выходит
            {
                tExit = std::min(tExit, t);
            }
            if (tEnter > tExit) return false;
        }

        if (enterFace == static_cast<size_t>(-1)) return false;   // origin внутри тела или тело позади
        outDistance = tEnter;
        outFace = enterFace;
        return true;
    }
}

bool RaycastSolid(const Solid& solid, const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
                  float& outDistance, size_t& outFace)
{
    const glm::dvec3 dir = glm::normalize(glm::dvec3(direction));
    double distance = 0.0;
    if (!RaycastSolidD(solid, glm::dvec3(origin), dir, maxDistance, distance, outFace)) return false;

    outDistance = static_cast<float>(distance);
    return true;
}

bool RaycastLevel(const LevelDocument& level, const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
                  LevelHit& out)
{
    const glm::dvec3 o(origin);
    const glm::dvec3 d = glm::normalize(glm::dvec3(direction));

    double bestDistance = maxDistance;
    bool found = false;

    for (size_t s = 0; s < level.solids.size(); ++s)
    {
        const Solid& solid = level.solids[s];
        if (!RayHitsBounds(SolidBounds(solid), o, d, bestDistance)) continue;

        double distance = 0.0;
        size_t face = 0;
        if (!RaycastSolidD(solid, o, d, bestDistance, distance, face)) continue;
        if (distance >= bestDistance && found) continue;

        bestDistance = distance;
        found = true;

        out.solidIndex = s;
        out.faceIndex = face;
        out.solidUid = solid.uid;
        out.faceUid = solid.faces[face].uid;
        out.distance = static_cast<float>(distance);
        out.position = glm::vec3(o + d * distance);
        out.normal = FaceNormal(solid, solid.faces[face]);
    }
    return found;
}

namespace
{
    // Грань считается видимой (обращённой к камере), если смотрит против луча. Перпендикулярная лучу не видна:
    // в ортографике боковые грани стоят ребром, и именно они нужны.
    constexpr double kFrontEpsilon = 1e-6;

    struct HorizonEdge
    {
        size_t solidIndex = 0;
        size_t dragFace = 0;               // невидимая грань пары: её и тащим
        size_t visibleFace = 0;
        double distance = 0.0;             // между лучом и ребром
        double rayDistance = 0.0;          // вдоль луча до ближайшей точки
        glm::dvec3 handlePoint{0.0};
        glm::dvec3 visibleNormal{0.0};
    };

    // Горизонтальные ребра солида: две смежные грани по разные стороны от силуэта
    void CollectHorizonEdges(const Solid& solid, size_t solidIndex, const Edit::Ray& ray, std::vector<HorizonEdge>& out)
    {
        struct EdgeFaces { int a = -1; int b = -1; };
        std::map<std::pair<uint32_t, uint32_t>, EdgeFaces> edges;

        for (size_t f = 0; f < solid.faces.size(); ++f)
        {
            const std::vector<uint32_t>& loop = solid.faces[f].loop;
            for (size_t i = 0; i < loop.size(); ++i)
            {
                const uint32_t u = loop[i];
                const uint32_t v = loop[(i + 1) % loop.size()];
                EdgeFaces& edge = edges[{ std::min(u, v), std::max(u, v) }];
                (u < v ? edge.a : edge.b) = static_cast<int>(f);
            }
        }

        std::vector<glm::dvec3> normals;
        std::vector<double> dots;
        normals.reserve(solid.faces.size());
        dots.reserve(solid.faces.size());
        for (const Face& face : solid.faces)
        {
            normals.push_back(glm::dvec3(FaceNormal(solid, face)));
            dots.push_back(glm::dot(normals.back(), ray.dir));
        }

        for (const auto& [key, adjacent] : edges)
        {
            if (adjacent.a < 0 || adjacent.b < 0) continue;

            const bool frontA = dots[adjacent.a] < -kFrontEpsilon;
            const bool frontB = dots[adjacent.b] < -kFrontEpsilon;
            if (frontA == frontB) continue;   // обе видны или обе нет: ребро не на силуэте

            const Edit::SegmentApproach approach = Edit::ClosestRaySegment(ray, glm::dvec3(solid.vertices[key.first]),
                                                                                glm::dvec3(solid.vertices[key.second]));
            HorizonEdge edge;
            edge.solidIndex = solidIndex;
            edge.dragFace = static_cast<size_t>(frontA ? adjacent.b : adjacent.a);
            edge.visibleFace = static_cast<size_t>(frontA ? adjacent.a : adjacent.b);
            edge.distance = approach.distance;
            edge.rayDistance = approach.rayT;
            edge.handlePoint = approach.segmentPoint;
            edge.visibleNormal = normals[edge.visibleFace];
            out.push_back(edge);
        }
    }
}

bool PickExtrudeFace(const std::vector<const Solid*>& solids, const Edit::Ray& ray, bool orthographic,
                     double edgeHandleRadius, ExtrudePick& out)
{
    // Прямое попадание: ближайшая входная грань среди выбранных солидов (остальной уровень луч не заслоняет)
    bool direct = false;
    ExtrudePick directPick;
    for (size_t s = 0; s < solids.size(); ++s)
    {
        float distance = 0.0f;
        size_t face = 0;
        if (!RaycastSolid(*solids[s], glm::vec3(ray.origin), glm::vec3(ray.dir), static_cast<float>(ray.maxDistance), distance, face)) continue;
        if (direct && distance >= directPick.distance) continue;

        direct = true;
        directPick.kind = ExtrudePick::Kind::Face;
        directPick.solidIndex = s;
        directPick.face = face;
        directPick.distance = distance;
        directPick.handlePoint = ray.at(distance);
    }

    if (direct && !orthographic)
    {
        out = directPick;
        return true;
    }

    std::vector<HorizonEdge> horizon;
    for (size_t s = 0; s < solids.size(); ++s) CollectHorizonEdges(*solids[s], s, ray, horizon);

    const HorizonEdge* best = nullptr;
    for (const HorizonEdge& edge : horizon)
    {
        // Ортографика, курсор внутри силуэта: работает только ребро, у которого курсор в пределах ручки
        if (direct && orthographic && edge.distance > edgeHandleRadius) continue;
        if (!best || edge.distance < best->distance) best = &edge;
    }
    if (!best) return false;   // нет горизонтальных рёбер, либо 2D и курсор внутри силуэта вдали от рёбер

    out = {};
    out.kind = ExtrudePick::Kind::Horizon;
    out.solidIndex = best->solidIndex;
    out.face = best->dragFace;
    out.distance = best->rayDistance;
    out.handlePoint = best->handlePoint;
    out.referenceNormal = orthographic ? ray.dir : best->visibleNormal;
    return true;
}
