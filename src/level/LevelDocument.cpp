//
//  LevelDocument.cpp
//  Cucumber
//

#include "LevelDocument.h"

#include "geometry/Epsilon.h"
#include "geometry/Polygon.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <random>

uint32_t LevelDocument::materialIndex(const std::string& path)
{
    for (size_t i = 0; i < materials.size(); ++i)
    {
        if (materials[i] == path) return static_cast<uint32_t>(i);
    }
    materials.push_back(path);
    return static_cast<uint32_t>(materials.size() - 1);
}

uint64_t NewUid()
{
    static std::mt19937_64 engine{std::random_device{}()};
    uint64_t uid = 0;
    while (uid == 0) uid = engine();
    return uid;
}

std::string UidToString(uint64_t uid)
{
    char buffer[17];
    std::snprintf(buffer, sizeof(buffer), "%016llx", static_cast<unsigned long long>(uid));
    return buffer;
}

bool ParseUid(const std::string& text, uint64_t& uid)
{
    if (text.empty() || text.size() > 16) return false;

    uint64_t value = 0;
    for (char c : text)
    {
        value <<= 4;
        if (c >= '0' && c <= '9')      value |= static_cast<uint64_t>(c - '0');
        else if (c >= 'a' && c <= 'f') value |= static_cast<uint64_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') value |= static_cast<uint64_t>(c - 'A' + 10);
        else return false;
    }
    uid = value;
    return true;
}

namespace
{
    Geo::Polygon FacePolygon(const Solid& solid, const Face& face)
    {
        Geo::Polygon polygon;
        polygon.reserve(face.loop.size());
        for (uint32_t index : face.loop) polygon.push_back(Geo::Vec3(solid.vertices[index]));
        return polygon;
    }
}

glm::vec3 FaceNormal(const Solid& solid, const Face& face)
{
    const Geo::Vec3 n = Geo::NewellNormal(FacePolygon(solid, face));
    const double length = glm::length(n);
    return length > 1e-12 ? glm::vec3(n / length) : glm::vec3(0.0f, 1.0f, 0.0f);
}

AABB SolidBounds(const Solid& solid)
{
    AABB bounds;
    for (const glm::vec3& v : solid.vertices) bounds.expand(v);
    return bounds;
}

void CompactVertices(Solid& solid)
{
    std::vector<int> remap(solid.vertices.size(), -1);
    std::vector<glm::vec3> kept;

    for (Face& face : solid.faces)
    {
        for (uint32_t& index : face.loop)
        {
            if (remap[index] < 0)
            {
                remap[index] = static_cast<int>(kept.size());
                kept.push_back(solid.vertices[index]);
            }
            index = static_cast<uint32_t>(remap[index]);
        }
    }
    solid.vertices = std::move(kept);
}

bool ValidateSolid(const Solid& solid, size_t materialCount, std::string& error)
{
    const auto fail = [&](const std::string& message) { error = message; return false; };

    if (solid.uid == 0) return fail("solid has no uid");
    if (solid.vertices.size() < 4) return fail("solid has fewer than 4 vertices");
    if (solid.faces.size() < 4) return fail("solid has fewer than 4 faces");

    // Направленные рёбра: каждое должно встретиться один раз, и обратное к нему тоже один раз
    std::map<std::pair<uint32_t, uint32_t>, int> edges;

    for (size_t f = 0; f < solid.faces.size(); ++f)
    {
        const Face& face = solid.faces[f];
        const std::string where = "face " + std::to_string(f) + ": ";

        if (face.uid == 0) return fail(where + "no uid");
        if (face.loop.size() < 3) return fail(where + "fewer than 3 vertices");
        if (face.material >= materialCount) return fail(where + "material index out of range");

        std::vector<uint32_t> sorted = face.loop;
        std::sort(sorted.begin(), sorted.end());
        if (sorted.back() >= solid.vertices.size()) return fail(where + "vertex index out of range");
        if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end()) return fail(where + "repeated vertex");

        const Geo::Polygon polygon = FacePolygon(solid, face);
        if (Geo::Area(polygon) < Geo::kMinArea) return fail(where + "degenerate (zero area)");
        if (!Geo::IsPlanar(polygon, Geo::kPlanarity)) return fail(where + "not planar");

        // Все вершины тела позади плоскости грани: и выпуклость, и нормаль наружу
        const Geo::Vec3 normal = glm::normalize(Geo::NewellNormal(polygon));
        const double distance = glm::dot(normal, Geo::Centroid(polygon));
        for (const glm::vec3& v : solid.vertices)
        {
            if (glm::dot(normal, Geo::Vec3(v)) - distance > Geo::kPlanarity)
                return fail(where + "solid is not convex or the face points inward");
        }

        for (size_t i = 0; i < face.loop.size(); ++i)
        {
            ++edges[{face.loop[i], face.loop[(i + 1) % face.loop.size()]}];
        }
    }

    for (const auto& [edge, count] : edges)
    {
        if (count != 1) return fail("edge shared by two faces in the same direction");

        const auto reverse = edges.find({edge.second, edge.first});
        if (reverse == edges.end() || reverse->second != 1) return fail("surface is not closed (open edge)");
    }

    return true;
}
