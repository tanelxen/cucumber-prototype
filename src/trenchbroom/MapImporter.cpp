//
//  MapImporter.cpp
//  Cucumber
//

#include "MapImporter.h"

#include "MapAsset.h"
#include "geometry/Brush.h"
#include "geometry/Epsilon.h"

namespace
{
    // Quake (Z вверх) -> движок (Y вверх). Это поворот (определитель +1), порядок обхода граней сохраняется.
    glm::dvec3 QuakeToEngine(const glm::dvec3& q) { return { q.x, q.z, -q.y }; }
    glm::vec3 QuakeToEngine(const glm::vec3& q)   { return { q.x, q.z, -q.y }; }

    bool IsNoDrawTexture(const std::string& name)
    {
        return name == "__TB_empty" || name == "skip" || name == "clip";
    }

    // splitmix64: детерминированные UID из номера браша и стороны
    uint64_t Mix(uint64_t x)
    {
        x += 0x9E3779B97F4A7C15ull;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
        return x ^ (x >> 31);
    }

    uint64_t NonZero(uint64_t x) { return x == 0 ? 1 : x; }
}

bool ImportMapAsset(const MapAsset& mapData, LevelDocument& out, MapImportReport& report, std::string& error)
{
    out = LevelDocument{};
    report = {};

    if (mapData.entities.empty())
    {
        error = "Map has no worldspawn entity";
        return false;
    }

    out.basePath = mapData.m_basepath;
    report.entitiesIgnored = static_cast<int>(mapData.entities.size()) - 1;

    const auto& worldspawn = mapData.entities[0];

    for (size_t brushIndex = 0; brushIndex < worldspawn.brushes.size(); ++brushIndex)
    {
        const auto& brush = worldspawn.brushes[brushIndex];
        ++report.brushesRead;

        const auto skip = [&](const std::string& why)
        {
            ++report.brushesSkipped;
            report.warnings.push_back("brush " + std::to_string(brushIndex) + ": " + why);
        };

        if (brush.sides.size() < 4) { skip("fewer than 4 sides"); continue; }

        // Плоскости. Нормали нормализуем: плоскость должна быть единичной, иначе допуски теряют смысл.
        std::vector<Geo::Plane> planes;
        planes.reserve(brush.sides.size());
        bool badPlane = false;
        for (const auto& side : brush.sides)
        {
            const glm::dvec3 n(side.plane.normal);
            const double length = glm::length(n);
            if (length < 1e-9) { badPlane = true; break; }
            planes.push_back({ n / length, static_cast<double>(side.plane.distance) / length });
        }
        if (badPlane) { skip("side with a zero normal"); continue; }

        std::vector<Geo::Polygon> polygons;
        if (!Geo::BuildBrushPolygons(planes, polygons)) { skip("sides do not enclose a volume"); continue; }

        for (Geo::Polygon& polygon : polygons)
        {
            for (Geo::Vec3& p : polygon) p = QuakeToEngine(p);
        }

        std::vector<Geo::Vec3> vertices;
        std::vector<std::vector<uint32_t>> loops;
        Geo::IndexPolygons(polygons, Geo::kWeld, vertices, loops);

        Solid solid;
        solid.uid = NonZero(Mix(brushIndex + 1));
        for (const Geo::Vec3& v : vertices) solid.vertices.push_back(glm::vec3(v));

        for (size_t sideIndex = 0; sideIndex < brush.sides.size(); ++sideIndex)
        {
            if (loops[sideIndex].empty()) { ++report.facesDropped; continue; }

            const auto& side = brush.sides[sideIndex];

            Face face;
            face.uid = NonZero(Mix((brushIndex + 1) * 1000003ull + sideIndex + 1));
            face.loop = loops[sideIndex];
            face.material = out.materialIndex(side.texture.textureName);
            face.uv.uAxis = QuakeToEngine(side.texture.uAxis);   // оси преобразуются как векторы; shift и scale не меняются
            face.uv.vAxis = QuakeToEngine(side.texture.vAxis);
            face.uv.shift = { side.texture.uShift, side.texture.vShift };
            face.uv.scale = { side.texture.scaleX, side.texture.scaleY };
            if (IsNoDrawTexture(side.texture.textureName)) face.flags |= FaceFlag::NoDraw;

            solid.faces.push_back(std::move(face));
        }

        CompactVertices(solid);

        std::string reason;
        if (!ValidateSolid(solid, out.materials.size(), reason)) { skip(reason); continue; }

        out.solids.push_back(std::move(solid));
        ++report.brushesImported;
    }

    if (out.solids.empty())
    {
        error = "Map has no valid brushes";
        return false;
    }

    error.clear();
    return true;
}
