//
//  FaceTexturing.cpp
//  Cucumber
//

#include "FaceTexturing.h"

#include "SolidBuilder.h"

#include <algorithm>
#include <cmath>

namespace FaceTexturing
{
    namespace
    {
        constexpr float kPi = 3.14159265358979323846f;
        constexpr float kMinScale = 0.001f;

        glm::vec3 RotateAround(const glm::vec3& v, const glm::vec3& axis, float radians)
        {
            // Формула Родрига: правая тройка вокруг axis (единичный)
            const float c = std::cos(radians);
            const float s = std::sin(radians);
            return v * c + glm::cross(axis, v) * s + axis * glm::dot(axis, v) * (1.0f - c);
        }

        glm::vec3 ProjectOntoPlane(const glm::vec3& v, const glm::vec3& normal)
        {
            const glm::vec3 p = v - normal * glm::dot(v, normal);
            const float length = glm::length(p);
            return length > 1e-6f ? p / length : glm::vec3(0.0f);
        }
    }

    float NormalizeDegrees(float degrees)
    {
        degrees = std::fmod(degrees, 360.0f);
        if (degrees > 180.0f) degrees -= 360.0f;
        if (degrees <= -180.0f) degrees += 360.0f;
        return degrees;
    }

    float Rotation(const glm::vec3& normal, const FaceUV& uv)
    {
        const glm::vec3 n = glm::normalize(normal);
        const glm::vec3 reference = ProjectOntoPlane(DefaultFaceUV(n).uAxis, n);
        const glm::vec3 current = ProjectOntoPlane(uv.uAxis, n);
        if (glm::length(reference) < 0.5f || glm::length(current) < 0.5f) return 0.0f;

        const float angle = std::atan2(glm::dot(glm::cross(reference, current), n), glm::dot(reference, current));
        return NormalizeDegrees(angle * 180.0f / kPi);
    }

    FaceUV Rotated(const glm::vec3& normal, const FaceUV& uv, float deltaDegrees)
    {
        const glm::vec3 n = glm::normalize(normal);
        const float radians = deltaDegrees * kPi / 180.0f;

        FaceUV result = uv;
        result.uAxis = RotateAround(uv.uAxis, n, radians);
        result.vAxis = RotateAround(uv.vAxis, n, radians);
        return result;
    }

    FaceUV WithRotation(const glm::vec3& normal, const FaceUV& uv, float degrees)
    {
        return Rotated(normal, uv, degrees - Rotation(normal, uv));
    }

    FaceUV FlippedU(const FaceUV& uv)
    {
        FaceUV result = uv;
        result.scale.x = -result.scale.x;
        return result;
    }

    FaceUV FlippedV(const FaceUV& uv)
    {
        FaceUV result = uv;
        result.scale.y = -result.scale.y;
        return result;
    }

    FaceUV Shifted(const FaceUV& uv, const glm::vec2& deltaTexels)
    {
        FaceUV result = uv;
        result.shift += deltaTexels;
        return result;
    }

    FaceUV Reset(const glm::vec3& normal)
    {
        return DefaultFaceUV(glm::normalize(normal));
    }

    glm::vec2 Texels(const FaceUV& uv, const glm::vec3& position)
    {
        return { glm::dot(position, uv.uAxis) / uv.scale.x + uv.shift.x,
                 glm::dot(position, uv.vAxis) / uv.scale.y + uv.shift.y };
    }

    FaceUV Fit(const Solid& solid, const Face& face, const glm::ivec2& textureSize, const glm::vec2& repeat)
    {
        FaceUV uv = face.uv;
        if (face.loop.empty()) return uv;

        glm::vec2 lo(1e30f), hi(-1e30f);
        for (uint32_t index : face.loop)
        {
            const glm::vec3& p = solid.vertices[index];
            const glm::vec2 raw(glm::dot(p, uv.uAxis), glm::dot(p, uv.vAxis));   // до деления на scale
            lo = glm::min(lo, raw);
            hi = glm::max(hi, raw);
        }

        const glm::vec2 extent = hi - lo;
        const glm::vec2 texels(std::max(1, textureSize.x) * std::max(repeat.x, 0.0001f),
                               std::max(1, textureSize.y) * std::max(repeat.y, 0.0001f));

        for (int axis = 0; axis < 2; ++axis)
        {
            if (extent[axis] < 1e-4f) continue;   // грань без протяжённости вдоль этой оси: оставляем как есть

            const float sign = uv.scale[axis] < 0.0f ? -1.0f : 1.0f;                     // зеркало сохраняется
            uv.scale[axis] = sign * extent[axis] / texels[axis];
            uv.shift[axis] = -(sign > 0.0f ? lo[axis] : hi[axis]) / uv.scale[axis];      // минимум u (или v) приходится на 0
        }
        return uv;
    }

    // ---------------------------------------------------------------- команды

    bool SetMaterial(LevelEditSession& session, const std::vector<FaceRef>& targets, const std::string& materialName)
    {
        const uint32_t index = session.document().materialIndex(materialName);   // добавляет в таблицу, если материала ещё нет
        return session.editFaces(targets, "Set material", {}, [&](const Solid&, Face& face)
        {
            face.material = index;
            return true;
        });
    }

    bool SetFlag(LevelEditSession& session, const std::vector<FaceRef>& targets, uint32_t flag, bool enabled)
    {
        return session.editFaces(targets, "Change face flags", {}, [&](const Solid&, Face& face)
        {
            face.flags = enabled ? (face.flags | flag) : (face.flags & ~flag);
            return true;
        });
    }

    bool SetShift(LevelEditSession& session, const std::vector<FaceRef>& targets, const glm::vec2& shift, const std::string& tag)
    {
        return session.editFaces(targets, "Set texture offset", tag, [&](const Solid&, Face& face)
        {
            face.uv.shift = shift;
            return true;
        });
    }

    bool SetScale(LevelEditSession& session, const std::vector<FaceRef>& targets, const glm::vec2& scale, const std::string& tag)
    {
        // Нулевой масштаб ломает проекцию (деление на ноль): прижимаем к минимуму, знак (зеркало) сохраняем
        glm::vec2 safe = scale;
        for (int axis = 0; axis < 2; ++axis)
        {
            if (std::abs(safe[axis]) < kMinScale) safe[axis] = std::copysign(kMinScale, safe[axis] == 0.0f ? 1.0f : safe[axis]);
        }

        return session.editFaces(targets, "Set texture scale", tag, [&](const Solid&, Face& face)
        {
            face.uv.scale = safe;
            return true;
        });
    }

    bool SetRotation(LevelEditSession& session, const std::vector<FaceRef>& targets, float degrees, const std::string& tag)
    {
        return session.editFaces(targets, "Set texture rotation", tag, [&](const Solid& solid, Face& face)
        {
            face.uv = WithRotation(FaceNormal(solid, face), face.uv, degrees);
            return true;
        });
    }

    bool Nudge(LevelEditSession& session, const std::vector<FaceRef>& targets, const glm::vec2& deltaTexels)
    {
        return session.editFaces(targets, "Move texture", {}, [&](const Solid&, Face& face)
        {
            face.uv = Shifted(face.uv, deltaTexels);
            return true;
        });
    }

    bool Rotate(LevelEditSession& session, const std::vector<FaceRef>& targets, float deltaDegrees)
    {
        return session.editFaces(targets, "Rotate texture", {}, [&](const Solid& solid, Face& face)
        {
            face.uv = Rotated(FaceNormal(solid, face), face.uv, deltaDegrees);
            return true;
        });
    }

    bool Flip(LevelEditSession& session, const std::vector<FaceRef>& targets, bool alongU)
    {
        return session.editFaces(targets, "Flip texture", {}, [&](const Solid&, Face& face)
        {
            face.uv = alongU ? FlippedU(face.uv) : FlippedV(face.uv);
            return true;
        });
    }

    bool FitToFace(LevelEditSession& session, const std::vector<FaceRef>& targets, const SizeResolver& sizeOf, const glm::vec2& repeat)
    {
        return session.editFaces(targets, "Fit texture", {}, [&](const Solid& solid, Face& face)
        {
            const std::string& name = session.document().materials[face.material];
            face.uv = Fit(solid, face, sizeOf ? sizeOf(name) : glm::ivec2(1), repeat);
            return true;
        });
    }

    bool ResetUV(LevelEditSession& session, const std::vector<FaceRef>& targets)
    {
        return session.editFaces(targets, "Reset texture", {}, [&](const Solid& solid, Face& face)
        {
            face.uv = Reset(FaceNormal(solid, face));
            return true;
        });
    }

    // Раскладка для грани target, которая продолжает раскладку источника без шва.
    //
    // Целевую грань "разворачивают" вокруг линии пересечения плоскостей, пока она не ляжет в плоскость источника,
    // и берут там раскладку источника. Поворот R вокруг линии (ось axis, угол angle между нормалями):
    //     u(p) = dot(R(p - o) + o, uAxis) / scale.x + shift.x
    //          = dot(p, R^-1 uAxis) / scale.x + (shift.x + dot(o - R o, uAxis) / scale.x)
    // поэтому результат это та же проекция Valve220: оси повёрнуты на -angle, смещение скорректировано, масштаб прежний.
    // Оси остаются в плоскости цели (если лежали в плоскости источника), так что растяжения нет, а на самой линии
    // пересечения раскладка совпадает с исходной. Для соседних граней это линия их общего ребра.
    //
    // Нормали должны быть заданы одинаково (обе наружу или обе внутрь); offset = dot(normal, любая точка плоскости).
    FaceUV WrapUV(const FaceUV& source, const glm::vec3& sourceNormal, float sourceOffset,
                  const glm::vec3& targetNormal, float targetOffset)
    {
        const glm::vec3 ns = glm::normalize(sourceNormal);
        const glm::vec3 nt = glm::normalize(targetNormal);
        
        const glm::vec3 hinge = glm::cross(nt, ns);
        const float sinAngle = glm::length(hinge);
        
        // Плоскости параллельны (в том числе смотрят в противоположные стороны, как пол и потолок): проекция остаётся
        // рабочей, копии достаточно. Для одной плоскости она же даёт бесшовный рисунок, в том числе между разными брашами.
        if (sinAngle < 1e-4f) return source;
        
        const glm::vec3 axis = hinge / sinAngle;
        const float angle = std::atan2(sinAngle, glm::dot(nt, ns));   // 0..pi
        
        // Точка на линии пересечения плоскостей
        const glm::vec3 lineDirection = glm::cross(ns, nt);
        const glm::vec3 origin = (sourceOffset * glm::cross(nt, lineDirection) + targetOffset * glm::cross(lineDirection, ns))
        / glm::dot(lineDirection, lineDirection);
        const glm::vec3 translation = origin - RotateAround(origin, axis, angle);
        
        // Нулевой масштаб ValveUV трактует как 1: делаем так же, иначе смещение разойдётся с тем, что рисуется
        const float sx = source.scale.x != 0.0f ? source.scale.x : 1.0f;
        const float sy = source.scale.y != 0.0f ? source.scale.y : 1.0f;
        
        FaceUV result = source;
        result.uAxis = RotateAround(source.uAxis, axis, -angle);
        result.vAxis = RotateAround(source.vAxis, axis, -angle);
        result.shift.x += glm::dot(translation, source.uAxis) / sx;
        result.shift.y += glm::dot(translation, source.vAxis) / sy;
        return result;
    }
    
    bool Copy(const LevelEditSession& session, const FaceRef& source, FaceClipboard& out)
    {
        const Face* face = session.findFace(source);
        const Solid* solid = session.findSolid(source.solidUid);
        if (!face || !solid || face->loop.empty() || face->material >= session.document().materials.size()) return false;
        
        out.valid = true;
        out.materialName = session.document().materials[face->material];
        out.uv = face->uv;
        
        // Снимок плоскости источника на момент копирования: по нему Paste продолжает раскладку на других гранях.
        // Если источник потом сдвинут, вставка всё равно продолжает рисунок так, как он выглядел при копировании.
        out.normal = FaceNormal(*solid, *face);
        out.planeOffset = glm::dot(out.normal, solid->vertices[face->loop.front()]);
        return true;
    }
    
    bool Paste(LevelEditSession& session, const std::vector<FaceRef>& targets, const FaceClipboard& clipboard,
               bool material, bool uv, UVMode mode)
    {
        if (!clipboard.valid || (!material && !uv)) return false;
        
        const uint32_t index = material ? session.document().materialIndex(clipboard.materialName) : 0;
        return session.editFaces(targets, "Paste texture", {}, [&](const Solid& solid, Face& face)
                                 {
            if (material) face.material = index;
            if (uv)
            {
                if (mode == UVMode::Wrap && !face.loop.empty())
                {
                    const glm::vec3 normal = FaceNormal(solid, face);
                    const float offset = glm::dot(normal, solid.vertices[face.loop.front()]);
                    face.uv = WrapUV(clipboard.uv, clipboard.normal, clipboard.planeOffset, normal, offset);
                }
                else
                {
                    face.uv = clipboard.uv;   // та же мировая проекция на любой грани
                }
            }
            return true;
        });
    }

    
    bool Summarize(const LevelEditSession& session, const std::vector<FaceRef>& targets, Summary& out)
    {
        const Solid* firstSolid = session.findSolid(targets.front().solidUid);
        const Face* firstFace = session.findFace(targets.front());
        if (!firstSolid || !firstFace) return false;
        
        out.material = firstFace->material;
        out.uv = firstFace->uv;
        out.rotation = Rotation(FaceNormal(*firstSolid, *firstFace), firstFace->uv);
        
        for (const FaceRef& ref : targets)
        {
            const Solid* solid = session.findSolid(ref.solidUid);
            const Face* face = session.findFace(ref);
            if (!solid || !face) continue;
            
            ++out.total;
            if (face->flags & FaceFlag::NoDraw) ++out.noDrawCount;
            
            out.mixedMaterial = out.mixedMaterial || face->material != out.material;
            out.mixedShift = out.mixedShift || face->uv.shift != out.uv.shift;
            out.mixedScale = out.mixedScale || face->uv.scale != out.uv.scale;
            
            const float rotation = Rotation(FaceNormal(*solid, *face), face->uv);
            out.mixedRotation = out.mixedRotation || std::abs(NormalizeDegrees(rotation - out.rotation)) > 1e-3f;
        }
        return true;
    }
}
