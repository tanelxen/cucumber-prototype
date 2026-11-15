//
//  SolidBuilder.h
//  Cucumber
//
//  Построение и пересборка выпуклых солидов по плоскостям. Общая основа для создания бокса, push/pull граней
//  и (позже) правки вершин: грани переносят uid, материал и UV, меняется только форма.
//

#pragma once

#include "LevelDocument.h"
#include "geometry/Brush.h"

#include <string>
#include <vector>

// Плоскости граней солида (в порядке faces), нормали наружу
std::vector<Geo::Plane> SolidPlanes(const Solid& solid);

Geo::Vec3 FaceCentroid(const Solid& solid, const Face& face);

// UV по умолчанию для грани с данной нормалью: проекция вдоль доминирующей оси, масштаб 1
FaceUV DefaultFaceUV(const glm::vec3& outwardNormal);

// Пересобирает тело из плоскостей: planes[i] соответствует source.faces[i]. Грани сохраняют uid, материал, UV, флаги.
// false, если какая-то грань исчезла (схлопнулась) или результат не проходит ValidateSolid.
// Координаты округляются до 1/256, чтобы многократные правки не копили шум float (сетка остаётся ровной).
bool RebuildSolid(const Solid& source, const std::vector<Geo::Plane>& planes, size_t materialCount,
                  Solid& out, std::string& error);

// Новый осевой бокс. Грани получают новые uid и материал `material`.
bool MakeBoxSolid(const glm::dvec3& mins, const glm::dvec3& maxs, uint32_t material, size_t materialCount,
                  Solid& out, std::string& error);

// Пересобирает тело из новых положений его вершин (positions[i] соответствует source.vertices[i]).
// Грани строятся заново как выпуклая оболочка, поэтому грань, у которой вершина ушла из плоскости, распадается на
// треугольники, а грани, оказавшиеся в одной плоскости, сливаются. Каждая новая грань наследует материал, UV и флаги
// старой грани с наибольшим числом общих вершин (при равенстве с наиболее близкой нормалью); uid сохраняет лучшая
// из них, остальным выдаются новые. Порядок и число вершин сохраняются: индексы выбранных вершин остаются верными.
// false, если вершина ушла внутрь тела или две вершины слились, или точки не образуют объём.
bool RebuildSolidFromVertices(const Solid& source, const std::vector<glm::dvec3>& positions, size_t materialCount,
                              Solid& out, std::string& error);

// Копия солида, сдвинутая на delta. textureLock: текстура остаётся на месте относительно тела (компенсируется shift).
Solid TranslatedSolid(const Solid& solid, const glm::vec3& delta, bool textureLock);

// Копия с новыми uid солида и всех его граней (материалы, UV, флаги сохраняются)
Solid CloneWithNewUids(const Solid& solid);
