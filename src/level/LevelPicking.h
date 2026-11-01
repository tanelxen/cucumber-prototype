//
//  LevelPicking.h
//  Cucumber
//
//  Луч против геометрии документа. Физику (Jolt) для пикинга не используем: она не знает, какому солиду
//  и какой грани принадлежит попадание, а выпуклое тело проверяется точно и дёшево.
//

#pragma once

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
