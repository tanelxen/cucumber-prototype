//
//  LevelPicking.h
//  Cucumber
//
//  Луч против геометрии документа. Физику (Jolt) для пикинга не используем: она не знает, какому солиду
//  и какой грани принадлежит попадание, а выпуклое тело проверяется точно и дёшево.
//

#pragma once

#include "EditMath.h"
#include "LevelDocument.h"

#include <cstdint>
#include <vector>

struct LevelHit
{
    size_t solidIndex = 0;
    size_t faceIndex = 0;
    uint64_t solidUid = 0;
    uint64_t faceUid = 0;
    float distance = 0.0f;
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f};       // нормаль грани, наружу
};

// Луч против одного выпуклого тела. Попадание только снаружи: если origin внутри тела, возвращается false.
bool RaycastSolid(const Solid& solid, const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
                  float& outDistance, size_t& outFace);

// Ближайшее попадание по всему документу. Грани NoDraw тоже участвуют: clip-браши должны выбираться.
bool RaycastLevel(const LevelDocument& level, const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
                  LevelHit& out);

// Выбор грани для push/pull по схеме ExtrudeTool из TrenchBroom.
//
// Работает ТОЛЬКО с переданными солидами (выбранными): остальной уровень луч не заслоняет, поэтому грани, спрятанные
// за другими солидами, достижимы.
//
//   Перспектива: луч попал в солид: грань, через которую он входит. Иначе ближайшее к лучу "горизонтальное" ребро
//   (у одной смежной грани dot(нормаль, луч) < 0, то есть она видна, а у другой нет) и тыльная из двух граней
//   (та, что не видна). Радиуса нет: берётся просто ближайшее ребро.
//   Ортографика: луч попал в солид (курсор внутри силуэта): работает только у самого ребра, в пределах
//   edgeHandleRadius, иначе попадания нет. Вне силуэта берётся ближайшее горизонтальное ребро. Грань та же:
//   перпендикулярная виду или тыльная.
//
// Попадание внутрь силуэта в 2D намеренно не даёт грань: её нормаль идёт вдоль взгляда, и тащить её из этого вида нельзя.
struct ExtrudePick
{
    enum class Kind
    {
        Face,       // луч попал в грань: тащить вдоль прямой через handlePoint по нормали грани
        Horizon     // выбрана по горизонтальному ребру: тащить в опорной плоскости через handlePoint
    };

    Kind kind = Kind::Face;
    size_t solidIndex = 0;          // индекс в списке, переданном в PickExtrudeFace
    size_t face = 0;
    double distance = 0.0;          // расстояние вдоль луча (до попадания или до ближайшей точки ребра)
    glm::dvec3 handlePoint{0.0};    // точка захвата: попадание луча или ближайшая к лучу точка ребра

    // Horizon: нормаль опорной плоскости. В перспективе это нормаль видимой смежной грани, в ортографике направление
    // взгляда. Движение курсора в этой плоскости проецируется на нормаль выбранной грани: так перетаскивание не
    // вырождается, когда луч идёт почти вдоль нормали тыльной грани.
    glm::dvec3 referenceNormal{0.0, 0.0, 1.0};
};

// edgeHandleRadius: допуск "у самого ребра" в мировых единицах, нужен только в ортографике (см. выше)
bool PickExtrudeFace(const std::vector<const Solid*>& solids, const Edit::Ray& ray, bool orthographic,
                     double edgeHandleRadius, ExtrudePick& out);
