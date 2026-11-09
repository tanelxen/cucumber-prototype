//
//  LevelDocumentIO.cpp
//  Cucumber
//

#include "LevelDocumentIO.h"

//#include <nlohmann/json.hpp>
#include "../vendor/json.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>

namespace
{
    using Json = nlohmann::ordered_json;   // порядок ключей как при записи: uid первым

    constexpr const char* kFormat = "cucumber.level";

    // Кратчайшая десятичная запись float, которая читается обратно в ТОТ ЖЕ float (ровно тем путём, каким читает
    // загрузчик: разбор как double и приведение к float). Значения на сетке пишутся коротко (0.5, 128, 0.25),
    // остальные теми цифрами, которые нужны для точности. Округление до фиксированного числа знаков здесь нельзя:
    // ошибка в масштабе или оси UV умножается на координату и на удалении от начала уводит UV на целые тексели.
    double Exact(float value)
    {
        if (value == 0.0f) return 0.0;   // заодно убирает "-0"

        char buffer[40];
        for (int precision = 1; precision <= 9; ++precision)
        {
            std::snprintf(buffer, sizeof(buffer), "%.*g", precision, static_cast<double>(value));
            const double parsed = std::strtod(buffer, nullptr);
            if (static_cast<float>(parsed) == value) return parsed;
        }
        return static_cast<double>(value);   // 9 значащих цифр всегда достаточно
    }

    Json Vec3ToJson(const glm::vec3& v)
    {
        return Json::array({ Exact(v.x), Exact(v.y), Exact(v.z) });
    }

    Json Vec2ToJson(const glm::vec2& v)
    {
        return Json::array({ Exact(v.x), Exact(v.y) });
    }

    Json SolidToJson(const Solid& solid)
    {
        Json vertices = Json::array();
        for (const glm::vec3& v : solid.vertices) vertices.push_back(Vec3ToJson(v));

        Json faces = Json::array();
        for (const Face& face : solid.faces)
        {
            Json f;
            f["uid"] = UidToString(face.uid);
            f["loop"] = face.loop;
            f["material"] = face.material;

            Json uv;
            uv["uAxis"] = Vec3ToJson(face.uv.uAxis);
            uv["vAxis"] = Vec3ToJson(face.uv.vAxis);
            uv["shift"] = Vec2ToJson(face.uv.shift);
            uv["scale"] = Vec2ToJson(face.uv.scale);
            f["uv"] = std::move(uv);

            if (face.flags & FaceFlag::NoDraw) f["flags"] = Json::array({ "nodraw" });

            faces.push_back(std::move(f));
        }

        Json s;
        s["uid"] = UidToString(solid.uid);
        s["vertices"] = std::move(vertices);
        s["faces"] = std::move(faces);
        return s;
    }

    bool ReadVec3(const Json& j, glm::vec3& out)
    {
        if (!j.is_array() || j.size() != 3) return false;
        for (int i = 0; i < 3; ++i)
        {
            if (!j[i].is_number()) return false;
            out[i] = j[i].get<float>();
        }
        return true;
    }

    bool ReadVec2(const Json& j, glm::vec2& out)
    {
        if (!j.is_array() || j.size() != 2) return false;
        for (int i = 0; i < 2; ++i)
        {
            if (!j[i].is_number()) return false;
            out[i] = j[i].get<float>();
        }
        return true;
    }

    bool ReadSolid(const Json& j, Solid& solid, std::string& error)
    {
        const auto fail = [&](const std::string& message) { error = message; return false; };

        if (!j.is_object()) return fail("solid is not an object");

        std::string uidText = j.value("uid", "");
        if (!ParseUid(uidText, solid.uid)) return fail("bad or missing uid");

        if (!j.contains("vertices") || !j["vertices"].is_array()) return fail("missing vertices");
        for (const Json& v : j["vertices"])
        {
            glm::vec3 p;
            if (!ReadVec3(v, p)) return fail("bad vertex");
            solid.vertices.push_back(p);
        }

        if (!j.contains("faces") || !j["faces"].is_array()) return fail("missing faces");
        for (const Json& jf : j["faces"])
        {
            if (!jf.is_object()) return fail("face is not an object");

            Face face;
            if (!ParseUid(jf.value("uid", ""), face.uid)) return fail("face: bad or missing uid");

            if (!jf.contains("loop") || !jf["loop"].is_array()) return fail("face: missing loop");
            for (const Json& index : jf["loop"])
            {
                if (!index.is_number_unsigned()) return fail("face: bad loop index");
                face.loop.push_back(index.get<uint32_t>());
            }

            if (!jf.contains("material") || !jf["material"].is_number_unsigned()) return fail("face: bad material index");
            face.material = jf["material"].get<uint32_t>();

            if (jf.contains("uv"))
            {
                const Json& uv = jf["uv"];
                if (!uv.is_object()) return fail("face: bad uv");
                if (uv.contains("uAxis") && !ReadVec3(uv["uAxis"], face.uv.uAxis)) return fail("face: bad uAxis");
                if (uv.contains("vAxis") && !ReadVec3(uv["vAxis"], face.uv.vAxis)) return fail("face: bad vAxis");
                if (uv.contains("shift") && !ReadVec2(uv["shift"], face.uv.shift)) return fail("face: bad shift");
                if (uv.contains("scale") && !ReadVec2(uv["scale"], face.uv.scale)) return fail("face: bad scale");
            }

            if (jf.contains("flags"))
            {
                if (!jf["flags"].is_array()) return fail("face: bad flags");
                for (const Json& flag : jf["flags"])
                {
                    if (flag == "nodraw") face.flags |= FaceFlag::NoDraw;
                    // неизвестные флаги из более новых версий игнорируются
                }
            }

            solid.faces.push_back(std::move(face));
        }
        return true;
    }
}

bool LevelDocumentIO::ToString(const LevelDocument& document, std::string& out, std::string& error)
{
    for (size_t i = 0; i < document.solids.size(); ++i)
    {
        std::string reason;
        if (!ValidateSolid(document.solids[i], document.materials.size(), reason))
        {
            error = "Refusing to save: solid " + std::to_string(i) + " (" + UidToString(document.solids[i].uid) + "): " + reason;
            return false;
        }
    }

    std::ostringstream stream;
    
    stream << "{\n";
    stream << "  \"format\": " << Json(kFormat).dump() << ",\n";
    stream << "  \"version\": " << document.version << ",\n";
    
    if (!document.sourceMap.empty())
    {
        stream << "  \"source\": " << Json(document.sourceMap).dump() << ",\n";
    }
    
    stream << "  \"materials\": " << Json(document.materials).dump() << ",\n";
    stream << "  \"solids\": [\n";
    
    for (size_t i = 0; i < document.solids.size(); ++i)
    {
        stream << "    " << SolidToJson(document.solids[i]).dump() << (i + 1 < document.solids.size() ? "," : "") << "\n";
    }
    
    stream << "  ]\n";
    stream << "}\n";

    out = stream.str();
    return true;
}

bool LevelDocumentIO::FromString(const std::string& text, LevelDocument& out, std::string& error)
{
    out = LevelDocument{};

    const Json root = Json::parse(text, nullptr, /*allow_exceptions*/ false);
    if (root.is_discarded() || !root.is_object())
    {
        error = "Level file is not valid JSON";
        return false;
    }

    if (root.value("format", "") != kFormat)
    {
        error = "Not a cucumber.level document";
        return false;
    }

    const uint32_t version = root.value("version", 0u);
    if (version == 0 || version > LevelDocument::kCurrentVersion)
    {
        error = "Unsupported level version " + std::to_string(version) + " (this build reads up to "
              + std::to_string(LevelDocument::kCurrentVersion) + ")";
        return false;
    }
    out.version = version;
    
    out.sourceMap = root.value("source", "");

    if (!root.contains("materials") || !root["materials"].is_array())
    {
        error = "Level file has no materials table";
        return false;
    }
    for (const Json& m : root["materials"])
    {
        if (!m.is_string()) { error = "Material path is not a string"; return false; }
        out.materials.push_back(m.get<std::string>());
    }

    if (!root.contains("solids") || !root["solids"].is_array())
    {
        error = "Level file has no solids array";
        return false;
    }

    for (size_t i = 0; i < root["solids"].size(); ++i)
    {
        Solid solid;
        std::string reason;
        if (!ReadSolid(root["solids"][i], solid, reason) || !ValidateSolid(solid, out.materials.size(), reason))
        {
            error = "Solid " + std::to_string(i) + ": " + reason;
            out = LevelDocument{};
            return false;
        }
        out.solids.push_back(std::move(solid));
    }

    error.clear();
    return true;
}

bool LevelDocumentIO::Save(const std::filesystem::path& path, const LevelDocument& document, std::string& error)
{
    std::string text;
    if (!ToString(document, text, error)) return false;

    // Через временный файл: оборванная запись не портит существующий документ
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) { error = "Cannot open " + temporary.string() + " for writing"; return false; }
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream) { error = "Write failed: " + temporary.string(); return false; }
    }

    std::error_code code;
    std::filesystem::rename(temporary, path, code);
    if (code)
    {
        std::filesystem::remove(path, code);
        std::filesystem::rename(temporary, path, code);
        if (code) { error = "Cannot replace " + path.string() + ": " + code.message(); return false; }
    }

    error.clear();
    return true;
}

bool LevelDocumentIO::Load(const std::filesystem::path& path, LevelDocument& out, std::string& error)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        out = LevelDocument{};
        error = "Cannot open " + path.string();
        return false;
    }

    std::ostringstream buffer;
    buffer << stream.rdbuf();

    if (!FromString(buffer.str(), out, error)) return false;

    out.basePath = path.parent_path();
    return true;
}
