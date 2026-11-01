//
//  EditorCamera.h
//  Cucumber
//
//  Заменяет CameraController + Camera для редактора. Один экземпляр на панель.
//  Умеет перспективу и ортографические пресеты (Top / Front / Side).
//  Ничего не знает про курсор и звук: панель сама решает, что делать с capture.
//

#pragma once

#include "ViewDesc.h"
#include "Input.h"

enum class EditorViewMode
{
    Perspective = 0,
    Top,
    Front,
    Side
};

class EditorCamera
{
public:
    void setMode(EditorViewMode mode);
    EditorViewMode mode() const { return m_mode; }
    bool isOrtho() const { return m_mode != EditorViewMode::Perspective; }

    // scrollWheel — шаги колеса (0 если мышь не над видом).
    // viewportHeightPx — высота вида в пикселях (для перевода пикселей мыши в единицы мира в ortho).
    void update(float dt, const Input::State& input, float scrollWheel, int viewportHeightPx, bool captureMouse);

    // true, пока активен режим полёта. Панель по этому флагу захватывает курсор.
    bool isCapturingMouse() const { return m_capturing; }

    CameraData build(float aspect) const;
    void focusOn(const glm::vec3& point);

    // Перспектива
    glm::vec3 position{0.0f, 128.0f, 128.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
    float fov = 55.0f;
    float moveSpeed = 400.0f;

    // Ортография
    glm::vec3 orthoCenter{0.0f};
    float orthoHalfHeight = 512.0f;

private:
    struct Basis { glm::vec3 forward, right, up; };
    Basis basis() const;

    void updatePerspective(float dt, const Input::State& input);
    void updateOrtho(const Input::State& input, float scrollWheel, int viewportHeightPx);

    EditorViewMode m_mode = EditorViewMode::Perspective;
    bool m_capturing = false;
};
