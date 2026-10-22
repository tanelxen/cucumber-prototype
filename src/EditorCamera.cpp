//
//  EditorCamera.cpp
//  Cucumber
//

#include "EditorCamera.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kMouseSense = 0.15f;
    constexpr float kNear = 0.1f;
    constexpr float kFar = 8192.0f;
    constexpr float kOrthoDepth = 8192.0f; // симметричный диапазон [-d, d] вокруг orthoCenter
}

void EditorCamera::setMode(EditorViewMode mode)
{
    if (mode == m_mode) return;

    // Уходя из перспективы, центрируем ortho там, где стоял глаз.
    if (m_mode == EditorViewMode::Perspective)
    {
        orthoCenter = position;
    }
    m_mode = mode;
}

EditorCamera::Basis EditorCamera::basis() const
{
    switch (m_mode)
    {
        // Координаты уже в пространстве OpenGL (см. Quake2OpenGL): север карты = -Z.
        case EditorViewMode::Top:   return { {0.f, -1.f, 0.f}, {1.f, 0.f,  0.f}, {0.f, 0.f, -1.f} };
        case EditorViewMode::Front: return { {0.f, 0.f, -1.f}, {1.f, 0.f,  0.f}, {0.f, 1.f,  0.f} };
        case EditorViewMode::Side:  return { {-1.f, 0.f, 0.f}, {0.f, 0.f, -1.f}, {0.f, 1.f,  0.f} };
        case EditorViewMode::Perspective: break;
    }

    // Та же математика, что была в CameraController
    Basis b;
    b.forward = glm::normalize(glm::vec3(-std::sin(yaw) * std::cos(pitch),
                                          std::sin(pitch),
                                         -std::cos(yaw) * std::cos(pitch)));
    b.right = glm::normalize(glm::vec3(std::cos(yaw), 0.0f, -std::sin(yaw)));
    b.up = glm::normalize(glm::cross(b.right, b.forward));
    return b;
}

void EditorCamera::update(float dt, const Input::State& input, float scrollWheel, int viewportHeightPx, bool canStartCapture)
{
    // Уже летим — продолжаем, пока ПКМ зажата. Начать можно только с разрешения панели.
    m_capturing = input.isRightMouseButtonPressed() && (m_capturing || canStartCapture);
    
    if (isOrtho()) updateOrtho(input, scrollWheel, viewportHeightPx);
    else           updatePerspective(dt, input);
}

void EditorCamera::updatePerspective(float dt, const Input::State& input)
{
    if (m_capturing)
    {
        // NB: умножение смещения мыши на dt оставлено как было, но это зависимость
        // чувствительности от FPS. Когда будет время — убрать dt и подобрать kMouseSense.
        yaw   -= input.getMouseOffsetX() * kMouseSense * dt;
        pitch -= input.getMouseOffsetY() * kMouseSense * dt;
    }
    pitch = std::clamp(pitch, -1.5f, 1.5f);

    if (!m_capturing) return;

    const Basis b = basis();

    float forwardMove = 0.0f, rightMove = 0.0f, upMove = 0.0f;
    if (input.isKeyPressed(KEY_W))        forwardMove += 1.0f;
    if (input.isKeyPressed(KEY_S))        forwardMove -= 1.0f;
    if (input.isKeyPressed(KEY_D))        rightMove   += 1.0f;
    if (input.isKeyPressed(KEY_A))        rightMove   -= 1.0f;
    if (input.isKeyPressed(KEY_SPACE))    upMove      += 1.0f;
    if (input.isKeyPressed(GLFW_KEY_C))   upMove      -= 1.0f;

    position += (b.forward * forwardMove + b.right * rightMove + b.up * upMove) * moveSpeed * dt;
}

void EditorCamera::updateOrtho(const Input::State& input, float scrollWheel, int viewportHeightPx)
{
    if (scrollWheel != 0.0f)
    {
        orthoHalfHeight = std::clamp(orthoHalfHeight * std::pow(0.9f, scrollWheel), 8.0f, 32768.0f);
    }

    if (m_capturing)
    {
        // Предполагается, что getMouseOffset* — сырые пиксели кадра. Если там уже есть масштаб — поправь.
        const float unitsPerPixel = (2.0f * orthoHalfHeight) / float(std::max(1, viewportHeightPx));
        const Basis b = basis();

        // Содержимое "едет" за курсором, значит камера двигается в обратную сторону.
        orthoCenter += (-b.right * input.getMouseOffsetX() + b.up * input.getMouseOffsetY()) * unitsPerPixel;
    }
}

CameraData EditorCamera::build(float aspect) const
{
    aspect = aspect > 0.0001f ? aspect : 1.0f;

    const Basis b = basis();
    CameraData c;
    c.forward = b.forward;
    c.ortho = isOrtho();

    if (!c.ortho)
    {
        c.position = position;
        c.view = glm::lookAt(position, position + b.forward, b.up);
        c.proj = glm::perspective(glm::radians(fov), aspect, kNear, kFar);
        c.nearZ = kNear;
        c.farZ = kFar;
    }
    else
    {
        const float hh = orthoHalfHeight;
        const float hw = hh * aspect;
        c.position = orthoCenter;
        c.view = glm::lookAt(orthoCenter, orthoCenter + b.forward, b.up);
        c.proj = glm::ortho(-hw, hw, -hh, hh, -kOrthoDepth, kOrthoDepth);
        c.nearZ = -kOrthoDepth;
        c.farZ = kOrthoDepth;
    }
    return c;
}

void EditorCamera::focusOn(const glm::vec3& point)
{
    if (isOrtho()) orthoCenter = point;
    else           position = point - basis().forward * 256.0f;
}
