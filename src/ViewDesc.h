//
//  ViewDesc.h
//  Cucumber
//
//  Описание "вида": ЧТО рисовать (камера, слои) и КУДА (цель, размер).
//  Ничего не знает про сцену, команды рендера и OpenGL.
//

#pragma once

#include <cstdint>
#include <string>
#include <glm/glm.hpp>

class RenderTarget;

struct CameraData
{
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
    glm::vec3 position{0.0f};
    glm::vec3 forward{0.0f, 0.0f, -1.0f};
    float nearZ = 0.1f;
    float farZ = 8192.0f;
    bool ortho = false;

    glm::mat4 viewProj() const { return proj * view; }
};

enum class ClearMode : uint8_t
{
    ColorDepth, // базовый вид
    Depth,      // оверлей поверх предыдущего вида (оружие от первого лица)
    None        // ничего не чистим
};

using ViewFlagMask = uint32_t;

namespace ViewFlag
{
    constexpr ViewFlagMask None          = 0;
    constexpr ViewFlagMask Sky           = 1 << 0;
    constexpr ViewFlagMask Grid          = 1 << 1;
    constexpr ViewFlagMask Debug         = 1 << 2; // gizmo / debug draw
    constexpr ViewFlagMask GameUI        = 1 << 3;
    constexpr ViewFlagMask AudioListener = 1 << 4; // отсюда берём позицию слушателя
}

struct ViewDesc
{
    uint32_t id = 0;
    std::string debugName;

    CameraData camera;

    RenderTarget* target = nullptr;   // nullptr = backbuffer
    glm::ivec2 size{1, 1};            // ЛОГИЧЕСКИЙ размер цели (не умноженный на retina-scale)

    uint32_t layerMask = 0xFFFFFFFF;
    ClearMode clear = ClearMode::ColorDepth;
    glm::vec4 clearColor{0.01f, 0.01f, 0.01f, 1.0f};

    // Явный порядок рендера (меньше = раньше). Зависимости между целями выражаются им же:
    // камеры, рисующие в текстуру, получают отрицательный order и рендерятся до тех, кто её читает.
    int order = 0;

    ViewFlagMask flags = ViewFlag::Sky;
};

// Луч из пикселя вида. mouse — в пикселях относительно левого верхнего угла вида.
// Работает и для перспективы, и для ортографии (замена Viewport::screenPointToRay).
inline void ScreenPointToRay(const CameraData& cam, const glm::ivec2& size, const glm::vec2& mouse,
                             glm::vec3& outStart, glm::vec3& outDir)
{
    const float x = 2.0f * mouse.x / float(size.x) - 1.0f;
    const float y = 1.0f - 2.0f * mouse.y / float(size.y);

    const glm::mat4 inv = glm::inverse(cam.viewProj());
    glm::vec4 nearP = inv * glm::vec4(x, y, -1.0f, 1.0f);
    glm::vec4 farP  = inv * glm::vec4(x, y,  1.0f, 1.0f);
    nearP /= nearP.w;
    farP  /= farP.w;

    outDir = glm::normalize(glm::vec3(farP - nearP));
    outStart = cam.ortho ? glm::vec3(nearP) : cam.position;
}
