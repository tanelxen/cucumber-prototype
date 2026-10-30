//
//  LevelDocument.h
//  Cucumber
//
//  Авторитетная модель статической геометрии уровня (то, что правит редактор и что лежит в .cmap.json).
//  Не знает ни про OpenGL, ни про UI, ни про рендер.
//
//  Решения формата v1:
//   - Солид это ВЫПУКЛОЕ тело: общие вершины + грани как петли индексов. Плоскости не хранятся, они выводятся (Ньюэлл).
//   - Петля каждой грани идёт против часовой стрелки, если смотреть СНАРУЖИ (правая система, Y вверх).
//   - Координаты в пространстве движка (Y вверх). Пространство Quake существует только в импортёре .map.
//   - Материал это путь (строка в таблице materials), как в .map; грань хранит индекс в таблице.
//   - Half-edge связность не хранится: при необходимости строится в памяти.
//

#pragma once

#include "Bounds.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace FaceFlag
{
    constexpr uint32_t NoDraw = 1u << 0;   // грань не рисуется (skip/clip/__TB_empty), но участвует в форме тела и физике
}

// Проекционная раскладка UV в стиле Valve220 (в пространстве движка):
//   u = dot(pos, uAxis) / scale.x + shift.x,  v = dot(pos, vAxis) / scale.y + shift.y;  texCoord = (u / ширина, v / высота)
struct FaceUV
{
    glm::vec3 uAxis{1.0f, 0.0f, 0.0f};
    glm::vec3 vAxis{0.0f, -1.0f, 0.0f};
    glm::vec2 shift{0.0f};
    glm::vec2 scale{1.0f};
};

struct Face
{
    uint64_t uid = 0;                 // стабильная идентичность грани (ключ раскладки лайтмапы, undo, выбора)
    std::vector<uint32_t> loop;       // индексы в Solid::vertices
    uint32_t material = 0;            // индекс в LevelDocument::materials
    FaceUV uv;
    uint32_t flags = 0;               // FaceFlag
};

struct Solid
{
    uint64_t uid = 0;
    std::vector<glm::vec3> vertices;
    std::vector<Face> faces;
};

struct LevelSettings
{
    float gridSnap = 8.0f;            // шаг привязки к сетке в редакторе
};

struct LevelDocument
{
    static constexpr uint32_t kCurrentVersion = 1;

    uint32_t version = kCurrentVersion;
    LevelSettings settings;
    std::vector<std::string> materials;   // пути материалов (как в .map, относительно каталога текстур)
    std::vector<Solid> solids;

    // Не сериализуется: каталог документа (нужен, чтобы найти scripts/ с шейдерами материалов)
    std::filesystem::path basePath;

    // Индекс материала по пути; добавляет, если такого ещё нет
    uint32_t materialIndex(const std::string& path);
};

// --- Идентичность ---
uint64_t NewUid();                                       // случайный ненулевой
std::string UidToString(uint64_t uid);                   // 16 hex-символов
bool ParseUid(const std::string& text, uint64_t& uid);

// --- Производные данные ---
glm::vec3 FaceNormal(const Solid& solid, const Face& face);   // нормализованная, наружу (Ньюэлл)
AABB SolidBounds(const Solid& solid);

// Удаляет вершины, на которые не ссылается ни одна грань, и перенумеровывает петли
void CompactVertices(Solid& solid);

// Проверка корректности солида: >= 4 граней, плоские грани ненулевой площади, выпуклость и нормали наружу,
// замкнутая поверхность (каждое ребро делят ровно две грани в противоположных направлениях).
bool ValidateSolid(const Solid& solid, size_t materialCount, std::string& error);
