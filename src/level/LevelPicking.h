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

struct FacePick
{
    size_t face = 0;
    double distance = 0.0;     // расстояние вдоль луча (для выбора ближайшего из нескольких солидов)
    bool viaEdge = false;      // true: курсор вне силуэта, грань выбрана по ближайшему ребру
};

// Грань для push/pull. Луч попал в тело: грань, через которую он входит (спереди). Иначе ближайшее к лучу ребро
// (в пределах tolerance), и из двух смежных граней берётся та, в сторону нормали которой курсор ушёл от ребра.
// Поэтому задняя грань выбирается, когда курсор пересекает силуэт с её стороны.
bool PickResizeFace(const Solid& solid, const Edit::Ray& ray, const Edit::PickTolerance& tolerance, FacePick& out);
