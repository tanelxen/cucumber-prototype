//
//  SolidBuilder.cpp
//  Cucumber
//

#include "SolidBuilder.h"

#include "geometry/Epsilon.h"

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
