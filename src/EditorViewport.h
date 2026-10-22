//
//  EditorViewport.h
//  Cucumber
//
//  Один "глаз" редактора: своя камера, своя цель рендера, свои флаги.
//  Панелей можно создавать сколько угодно. Заменяет Viewport + CameraController для редактора.
//

#pragma once

#include "EditorCamera.h"
#include "RenderTarget.h"
#include "ViewDesc.h"

#include <vector>

class EditorViewport
{
public:
    explicit EditorViewport(uint32_t id) : m_id(id) {}

    uint32_t id() const { return m_id; }

    EditorCamera& camera() { return m_camera; }
    const EditorCamera& camera() const { return m_camera; }
    RenderTarget& target() { return m_target; }

    glm::ivec2 size() const { return { m_target.width(), m_target.height() }; }
    float aspect() const { return float(m_target.width()) / float(m_target.height()); }

    // Панель сообщает желаемый размер из ImGui (во время draw).
    // Реальный ресайз происходит в applyPendingSize() в начале СЛЕДУЮЩЕГО кадра, до рендера.
    void requestSize(int width, int height, float scale);
    void applyPendingSize();

    // Добавляет редакторский вид этого вьюпорта.
    void produceEditorView(std::vector<ViewDesc>& out, bool audioListener);

    bool drawGrid = true;
    bool drawGizmos = false;
    bool drawSkybox = true;

private:
    uint32_t m_id;
    EditorCamera m_camera;
    RenderTarget m_target;

    bool m_hasPending = false;
    int m_pendingW = 1;
    int m_pendingH = 1;
    float m_pendingScale = 1.0f;
};
