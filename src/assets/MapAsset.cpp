//
//  MapAsset.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 21.10.26.
//

#include "MapAsset.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Вспомогательные функции
// ---------------------------------------------------------------------------

// Убирает "//" комментарий, но только если он находится вне кавычек
static std::string StripComment(const std::string& line)
{
    bool inQuotes = false;
    for (size_t i = 0; i < line.size(); ++i)
    {
        if (line[i] == '"') inQuotes = !inQuotes;
        else if (!inQuotes && line[i] == '/' && i + 1 < line.size() && line[i + 1] == '/')
            return line.substr(0, i);
    }
    return line;
}

static std::string Trim(const std::string& s)
{
    const char* ws = " \t\r\n";
    size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) return {};
    size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

// Читает следующий символ-разделитель и проверяет, что он совпадает с ожидаемым
static bool Expect(std::istream& is, char expected)
{
    char c = 0;
    is >> c;
    return is && c == expected;
}

// Читает точку плоскости: ( x y z )
static bool ParsePoint(std::istream& is, glm::vec3& p)
{
    if (!Expect(is, '(')) return false;
    is >> p.x >> p.y >> p.z;
    return Expect(is, ')');
}

// Читает ось текстуры Valve 220: [ x y z shift ]
static bool ParseTextureAxis(std::istream& is, glm::vec3& axis, float& shift)
{
    if (!Expect(is, '[')) return false;
    is >> axis.x >> axis.y >> axis.z >> shift;
    return Expect(is, ']');
}

// Читает имя текстуры: с кавычками (может содержать пробелы) или без
static bool ParseTextureName(std::istream& is, std::string& name)
{
    is >> std::ws;
    if (is.peek() == '"')
    {
        is.get(); // открывающая кавычка
        std::getline(is, name, '"'); // читаем до закрывающей
        return static_cast<bool>(is);
    }
    is >> name;
    return static_cast<bool>(is) && !name.empty();
}

// Плоскость по трем точкам, заданным в порядке Quake (по часовой стрелке).
// Возвращает false для вырожденной плоскости (коллинеарные точки).
static bool PlaneFromPoints(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, MapPlane& plane)
{
    glm::vec3 n = glm::cross(p2 - p0, p1 - p0);
    float len = glm::length(n);
    if (len < 1e-6f) return false;
    
    plane.normal = n / len;
    plane.distance = glm::dot(plane.normal, p0);
    return true;
}

// Разбор строки грани:
// ( p0 ) ( p1 ) ( p2 ) texture [ ux uy uz offX ] [ vx vy vz offY ] rotation scaleX scaleY
static bool ParseBrushSide(const std::string& line, MapBrushSide& side)
{
    std::istringstream is(line);
    
    glm::vec3 p0, p1, p2;
    if (!ParsePoint(is, p0) || !ParsePoint(is, p1) || !ParsePoint(is, p2)) return false;
    if (!PlaneFromPoints(p0, p1, p2, side.plane)) return false;
    
    if (!ParseTextureName(is, side.texture.textureName)) return false;
    
    if (!ParseTextureAxis(is, side.texture.uAxis, side.texture.uShift)) return false;
    if (!ParseTextureAxis(is, side.texture.vAxis, side.texture.vShift)) return false;
    
    // В Valve 220 поворот уже заложен в оси, rotation только читаем и игнорируем
    float rotation = 0.0f;
    is >> rotation >> side.texture.scaleX >> side.texture.scaleY;
    if (!is) return false;
    
    // Нулевой масштаб TrenchBroom трактует как 1
    if (std::abs(side.texture.scaleX) < 0.001f) side.texture.scaleX = 1.0f;
    if (std::abs(side.texture.scaleY) < 0.001f) side.texture.scaleY = 1.0f;
    
    return true;
}

// Разбор свойства энтити: "key" "value"
static bool ParseKeyValue(const std::string& line, std::string& key, std::string& value)
{
    size_t q1 = line.find('"');
    if (q1 == std::string::npos) return false;
    size_t q2 = line.find('"', q1 + 1);
    if (q2 == std::string::npos) return false;
    size_t q3 = line.find('"', q2 + 1);
    if (q3 == std::string::npos) return false;
    size_t q4 = line.rfind('"');
    if (q4 <= q3) return false;
    
    key = line.substr(q1 + 1, q2 - q1 - 1);
    value = line.substr(q3 + 1, q4 - q3 - 1);
    return true;
}

// ---------------------------------------------------------------------------
// Загрузка
// ---------------------------------------------------------------------------

bool MapAsset::loadFromFile(const std::string& filename)
{
    m_filepath = filename;
    m_basepath = m_filepath.parent_path().parent_path();
    
    std::ifstream file(filename);
    if (!file.is_open()) return false;
    
    entities.clear();
    
    std::string rawLine;
    MapEntity* currentEntity = nullptr;
    MapBrush* currentBrush = nullptr;
    
    while (std::getline(file, rawLine))
    {
        const std::string line = Trim(StripComment(rawLine));
        if (line.empty()) continue;
        
        const char first = line.front();
        
        if (first == '{')
        {
            if (!currentEntity)
            {
                entities.emplace_back();
                currentEntity = &entities.back();
            }
            else if (!currentBrush)
            {
                currentEntity->brushes.emplace_back();
                currentBrush = &currentEntity->brushes.back();
            }
            continue;
        }
        
        if (first == '}')
        {
            if (currentBrush) currentBrush = nullptr;
            else if (currentEntity) currentEntity = nullptr;
            continue;
        }
        
        // Свойство энтити: "key" "value"
        if (currentEntity && !currentBrush && first == '"')
        {
            std::string key, value;
            if (ParseKeyValue(line, key, value))
                currentEntity->properties[key] = value;
            continue;
        }
        
        // Грань браша
        if (currentBrush && first == '(')
        {
            MapBrushSide side;
            if (ParseBrushSide(line, side))
                currentBrush->sides.push_back(std::move(side));
            continue;
        }
    }
    
    return true;
}
