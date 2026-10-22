//
//  ViewportPanel.h
//  Cucumber
//
//  Панель = окно ImGui + EditorViewport. Панелей может быть несколько (Perspective, Top, Front, ...).
//  Панель решает, ЧТО она показывает: редакторскую камеру или игру (если она — "Play viewport").
//

#pragma once

#include "EditorPanel.h"
#include "EditorViewport.h"
#include "ViewDesc.h"

#include <glm/glm.hpp>
#include <vector>

class Entity;
class WorldScene;

class ViewportPanel : public EditorPanel
{
public:
    ViewportPanel(const std::string& name, uint32_t id);
    ~ViewportPanel();

    void setEntity(Entity* entity);
    void setEditorCameraPosition(const glm::vec3& position);
    glm::vec3 getEditorCameraPosition() const;

    void update(float dt) override;
    void draw() override;

    // Вызывается Application раз в кадр после обновления сцены.
    void collectViews(WorldScene& scene, std::vector<ViewDesc>& out);

    EditorViewport& viewport() { return m_viewport; }
    bool isHovered() const { return m_isHovered; }
    bool isPlayViewport() const { return m_isPlayViewport; }

private:
    void syncCursor();

    EditorViewport m_viewport;
    Entity* m_entity = nullptr;

    // Эта панель "держит" запущенную игру: показывает игровую камеру и отдаёт ей ввод.
    bool m_isPlayViewport = false;

    bool m_windowOpen = true;     // результат ImGui::Begin (false — свёрнута / вкладка не видна)
    bool m_focused = false;
    bool m_isHovered = false;
    bool m_cursorCaptured = false;

    bool m_hasImageBounds = false;
    float m_imageLeft = 0, m_imageTop = 0, m_imageRight = 0, m_imageBottom = 0;
    
private:
    void updateOwnership();
    bool m_prevRightMouse = false;
};
