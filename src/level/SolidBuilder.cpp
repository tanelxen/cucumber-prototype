//
//  SolidBuilder.cpp
//  Cucumber
//

#include "SolidBuilder.h"

#include "geometry/Epsilon.h"

#include <algorithm>
#include <cmath>

namespace
{
    double CleanCoordinate(double value)
    {
        return std::round(value * 256.0) / 256.0;
    }
}

Geo::Vec3 FaceCentroid(const Solid& solid, const Face& face)
{
    Geo::Polygon polygon;
    polygon.reserve(face.loop.size());
    for (uint32_t index : face.loop) polygon.push_back(Geo::Vec3(solid.vertices[index]));
    return Geo::Centroid(polygon);
}

std::vector<Geo::Plane> SolidPlanes(const Solid& solid)
{
    std::vector<Geo::Plane> planes;
    planes.reserve(solid.faces.size());

    for (const Face& face : solid.faces)
    {
        Geo::Polygon polygon;
        polygon.reserve(face.loop.size());
        for (uint32_t index : face.loop) polygon.push_back(Geo::Vec3(solid.vertices[index]));

        const Geo::Vec3 normal = glm::normalize(Geo::NewellNormal(polygon));
        planes.push_back({ normal, glm::dot(normal, Geo::Centroid(polygon)) });
    }
    return planes;
}

FaceUV DefaultFaceUV(const glm::vec3& n)
{
    // Те же оси, что TrenchBroom даёт новым граням (Quake: пол u=X v=-Y, стена X u=Y v=-Z, стена Y u=X v=-Z),
    // переведённые в пространство движка (x, y, z) = (qx, qz, -qy).
    FaceUV uv;
    const glm::vec3 a = glm::abs(n);

    if (a.y >= a.x && a.y >= a.z)       { uv.uAxis = {1, 0, 0};  uv.vAxis = {0, 0, 1}; }    // пол / потолок
    else if (a.x >= a.z)                { uv.uAxis = {0, 0, -1}; uv.vAxis = {0, -1, 0}; }   // стены, смотрящие по X
    else                                { uv.uAxis = {1, 0, 0};  uv.vAxis = {0, -1, 0}; }   // стены, смотрящие по Z
    return uv;
}

bool RebuildSolid(const Solid& source, const std::vector<Geo::Plane>& planes, size_t materialCount,
                  Solid& out, std::string& error)
{
    if (planes.size() != source.faces.size()) { error = "Plane count differs from face count"; return false; }

    std::vector<Geo::Polygon> polygons;
    if (!Geo::BuildBrushPolygons(planes, polygons)) { error = "Planes do not enclose a volume"; return false; }

    for (Geo::Polygon& polygon : polygons)
    {
        if (polygon.size() < 3) { error = "A face collapsed"; return false; }
        for (Geo::Vec3& p : polygon) p = { CleanCoordinate(p.x), CleanCoordinate(p.y), CleanCoordinate(p.z) };
    }

    std::vector<Geo::Vec3> vertices;
    std::vector<std::vector<uint32_t>> loops;
    Geo::IndexPolygons(polygons, Geo::kWeld, vertices, loops);

    Solid result;
    result.uid = source.uid;
    for (const Geo::Vec3& v : vertices) result.vertices.push_back(glm::vec3(v));

    for (size_t i = 0; i < source.faces.size(); ++i)
    {
        if (loops[i].size() < 3) { error = "A face collapsed"; return false; }

        Face face = source.faces[i];
        face.loop = loops[i];
        result.faces.push_back(std::move(face));
    }

    CompactVertices(result);
    if (!ValidateSolid(result, materialCount, error)) return false;

    out = std::move(result);
    error.clear();
    return true;
}

bool MakeBoxSolid(const glm::dvec3& mins, const glm::dvec3& maxs, uint32_t material, size_t materialCount,
                  Solid& out, std::string& error)
{
    if (maxs.x <= mins.x || maxs.y <= mins.y || maxs.z <= mins.z)
    {
        error = "Box has an empty extent";
        return false;
    }

    const std::vector<Geo::Plane> planes = {
        {{ 1, 0, 0},  maxs.x}, {{-1, 0, 0}, -mins.x},
        {{ 0, 1, 0},  maxs.y}, {{ 0,-1, 0}, -mins.y},
        {{ 0, 0, 1},  maxs.z}, {{ 0, 0,-1}, -mins.z},
    };

    Solid shell;
    shell.uid = NewUid();
    for (const Geo::Plane& plane : planes)
    {
        Face face;
        face.uid = NewUid();
        face.material = material;
        face.uv = DefaultFaceUV(glm::vec3(plane.normal));
        shell.faces.push_back(std::move(face));
    }

    return RebuildSolid(shell, planes, materialCount, out, error);
}

bool RebuildSolidFromVertices(const Solid& source, const std::vector<glm::dvec3>& positions, size_t materialCount,
                              Solid& out, std::string& error)
{
    const size_t count = source.vertices.size();
    if (positions.size() != count) { error = "Vertex count differs"; return false; }

    std::vector<Geo::Vec3> points;
    points.reserve(count);
    for (const glm::dvec3& p : positions) points.push_back({ CleanCoordinate(p.x), CleanCoordinate(p.y), CleanCoordinate(p.z) });

    for (size_t i = 0; i < count; ++i)
    {
        for (size_t j = i + 1; j < count; ++j)
        {
            if (glm::distance(points[i], points[j]) <= Geo::kWeld) { error = "Two vertices merged"; return false; }
        }
    }

    std::vector<Geo::Polygon> hull;
    if (!Geo::ConvexHullPolygons(points, hull)) { error = "Vertices do not form a volume"; return false; }

    std::vector<Geo::Vec3> hullVertices;
    std::vector<std::vector<uint32_t>> hullLoops;
    Geo::IndexPolygons(hull, Geo::kWeld, hullVertices, hullLoops);

    // Вершины оболочки (пересечения плоскостей, с шумом double) возвращаем к исходным точкам: так координаты остаются круглыми
    constexpr double kMatch = Geo::kWeld * 5.0;
    std::vector<int> toSource(hullVertices.size(), -1);
    std::vector<bool> used(count, false);

    for (size_t h = 0; h < hullVertices.size(); ++h)
    {
        for (size_t i = 0; i < count; ++i)
        {
            if (glm::distance(hullVertices[h], points[i]) > kMatch) continue;
            if (used[i]) { error = "Vertices too close together"; return false; }
            toSource[h] = static_cast<int>(i);
            used[i] = true;
            break;
        }
        if (toSource[h] < 0) { error = "Unexpected hull vertex"; return false; }
    }
    for (size_t i = 0; i < count; ++i)
    {
        if (!used[i]) { error = "A vertex ended up inside the solid"; return false; }
    }

    // Нормали и вершины старых граней для наследования
    const std::vector<Geo::Plane> oldPlanes = SolidPlanes(source);

    struct NewFace
    {
        std::vector<uint32_t> loop;
        size_t parent = 0;
        int overlap = 0;
        double alignment = 0.0;
    };
    std::vector<NewFace> faces;

    for (const std::vector<uint32_t>& hullLoop : hullLoops)
    {
        if (hullLoop.size() < 3) continue;

        NewFace face;
        for (uint32_t h : hullLoop) face.loop.push_back(static_cast<uint32_t>(toSource[h]));

        Geo::Polygon polygon;
        for (uint32_t index : face.loop) polygon.push_back(points[index]);
        const Geo::Vec3 normal = glm::normalize(Geo::NewellNormal(polygon));

        int bestOverlap = -1;
        double bestAlignment = -2.0;
        for (size_t f = 0; f < source.faces.size(); ++f)
        {
            int overlap = 0;
            for (uint32_t index : face.loop)
            {
                overlap += std::count(source.faces[f].loop.begin(), source.faces[f].loop.end(), index) > 0 ? 1 : 0;
            }
            if (overlap < 2) overlap = 0;   // одна общая вершина ничего не значит: решает нормаль

            const double alignment = glm::dot(normal, oldPlanes[f].normal);
            if (overlap > bestOverlap || (overlap == bestOverlap && alignment > bestAlignment))
            {
                bestOverlap = overlap;
                bestAlignment = alignment;
                face.parent = f;
            }
        }
        face.overlap = bestOverlap;
        face.alignment = bestAlignment;
        faces.push_back(std::move(face));
    }

    // uid старой грани достаётся лучшему из её наследников
    std::vector<int> owner(source.faces.size(), -1);
    for (size_t n = 0; n < faces.size(); ++n)
    {
        int& best = owner[faces[n].parent];
        if (best < 0 || faces[n].overlap > faces[best].overlap ||
            (faces[n].overlap == faces[best].overlap && faces[n].alignment > faces[best].alignment))
        {
            best = static_cast<int>(n);
        }
    }

    Solid result;
    result.uid = source.uid;
    for (const Geo::Vec3& p : points) result.vertices.push_back(glm::vec3(p));

    for (size_t n = 0; n < faces.size(); ++n)
    {
        Face face = source.faces[faces[n].parent];
        face.loop = faces[n].loop;
        if (owner[faces[n].parent] != static_cast<int>(n)) face.uid = NewUid();
        result.faces.push_back(std::move(face));
    }

    if (!ValidateSolid(result, materialCount, error)) return false;

    out = std::move(result);
    error.clear();
    return true;
}

Solid TranslatedSolid(const Solid& solid, const glm::vec3& delta, bool textureLock)
{
    Solid result = solid;
    for (glm::vec3& v : result.vertices) v += delta;

    if (textureLock)
    {
        // u = dot(pos, uAxis) / scale.x + shift.x: после сдвига pos на delta u уходит на dot(delta, uAxis) / scale.x.
        // Вычитаем это из shift, и текстура остаётся на теле.
        for (Face& face : result.faces)
        {
            const float sx = face.uv.scale.x != 0.0f ? face.uv.scale.x : 1.0f;
            const float sy = face.uv.scale.y != 0.0f ? face.uv.scale.y : 1.0f;
            face.uv.shift.x -= glm::dot(delta, face.uv.uAxis) / sx;
            face.uv.shift.y -= glm::dot(delta, face.uv.vAxis) / sy;
        }
    }
    return result;
}

Solid CloneWithNewUids(const Solid& solid)
{
    Solid copy = solid;
    copy.uid = NewUid();
    for (Face& face : copy.faces) face.uid = NewUid();
    return copy;
}
