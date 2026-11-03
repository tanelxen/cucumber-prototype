//
//  FaceTexturing.h
//  Cucumber
//
//  Текстурирование граней: математика проекционной раскладки (Valve220) и команды для панели текстурирования.
//
//    u = dot(pos, uAxis) / scale.x + shift.x      (единицы: текселы)
//    v = dot(pos, vAxis) / scale.y + shift.y      texCoord = (u / ширина, v / высота)
//
//  scale это мировых единиц на тексел (больше scale: текстура крупнее), отрицательный scale зеркалит текстуру,
//  shift это смещение в текселах. Все команды идут через LevelEditSession::editFaces, то есть в историю undo.
//

#pragma once

#include "LevelEditSession.h"

#include <functional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace FaceTexturing
{
    // --- Математика ---

    float NormalizeDegrees(float degrees);                                  // в (-180, 180]

    // Поворот текстуры относительно осей по умолчанию для этой нормали. Положительный угол это поворот против
    // часовой стрелки, если смотреть на грань снаружи (правая тройка вокруг нормали).
    float Rotation(const glm::vec3& normal, const FaceUV& uv);
    FaceUV Rotated(const glm::vec3& normal, const FaceUV& uv, float deltaDegrees);
    FaceUV WithRotation(const glm::vec3& normal, const FaceUV& uv, float degrees);

    FaceUV FlippedU(const FaceUV& uv);
    FaceUV FlippedV(const FaceUV& uv);
    FaceUV Shifted(const FaceUV& uv, const glm::vec2& deltaTexels);
    FaceUV Reset(const glm::vec3& normal);                                  // оси по умолчанию, shift 0, scale 1

    // Текстура ложится на грань ровно repeat раз по каждой оси (1, 1: один раз на всю грань). Поворот и зеркало
    // сохраняются: меняются только scale и shift.
    FaceUV Fit(const Solid& solid, const Face& face, const glm::ivec2& textureSize, const glm::vec2& repeat);

    glm::vec2 Texels(const FaceUV& uv, const glm::vec3& position);          // (u, v) точки до деления на размер текстуры

    // --- Команды (к граням из session.textureTargets()) ---

    using SizeResolver = std::function<glm::ivec2(const std::string& materialName)>;

    struct FaceClipboard
    {
        bool valid = false;
        std::string materialName;
        FaceUV uv;
    };

    bool SetMaterial(LevelEditSession& session, const std::vector<FaceRef>& targets, const std::string& materialName);
    bool SetFlag(LevelEditSession& session, const std::vector<FaceRef>& targets, uint32_t flag, bool enabled);

    // tag: тег слияния записей undo для непрерывных правок (перетаскивание поля); пусто для разовых кнопок
    bool SetShift(LevelEditSession& session, const std::vector<FaceRef>& targets, const glm::vec2& shift, const std::string& tag = {});
    bool SetScale(LevelEditSession& session, const std::vector<FaceRef>& targets, const glm::vec2& scale, const std::string& tag = {});
    bool SetRotation(LevelEditSession& session, const std::vector<FaceRef>& targets, float degrees, const std::string& tag = {});

    bool Nudge(LevelEditSession& session, const std::vector<FaceRef>& targets, const glm::vec2& deltaTexels);
    bool Rotate(LevelEditSession& session, const std::vector<FaceRef>& targets, float deltaDegrees);
    bool Flip(LevelEditSession& session, const std::vector<FaceRef>& targets, bool alongU);
    bool FitToFace(LevelEditSession& session, const std::vector<FaceRef>& targets, const SizeResolver& sizeOf, const glm::vec2& repeat);
    bool ResetUV(LevelEditSession& session, const std::vector<FaceRef>& targets);

    bool Copy(const LevelEditSession& session, const FaceRef& source, FaceClipboard& out);
    bool Paste(LevelEditSession& session, const std::vector<FaceRef>& targets, const FaceClipboard& clipboard, bool material, bool uv);
}
