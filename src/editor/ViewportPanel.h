//
//  ViewportPanel.h
//  Cucumber
//
//  Панель = окно ImGui + EditorViewport. Панелей может быть несколько (Perspective, Top, Front, ...).
//  Панель решает, ЧТО она показывает: редакторскую камеру или игру (если она "Play viewport").
//
//  Сама панель только связывает части; детали вынесены:
//    ViewportMouseOwnership / RightMouseGesture   владение мышью и жест ПКМ        (ViewportMouse.h)
//    DrawViewportToolbar                          тулбар, Play / Stop              (ViewportToolbar.h)
//    LevelEditInput                               ввод редактора уровня            (LevelEditInput.h)
//    ViewportAssetDrop                            drag-and-drop ассетов            (ViewportAssetDrop.h)
//    ViewportPicking                              мировая позиция под курсором     (ViewportPicking.h)
//    ViewportGizmo                                гизмо перемещения                (ViewportGizmo.h)
//

#pragma once

#include "EditorPanel.h"
#include "EditorViewport.h"
#include "core/Input.h"
#include "renderer/ViewDesc.h"
#include "ViewportImageRect.h"
#include "ViewportMouse.h"

#include <glm/glm.hpp>
#include <vector>

class Entity;
class WorldScene;
class LevelEditSession;

class ViewportPanel : public EditorPanel
{
public:
    ViewportPanel(const std::string& name, uint32_t id);
    ~ViewportPanel() override = default;

    void setEntity(Entity* entity);
    void setEditorCameraPosition(const glm::vec3& position);
    glm::vec3 getEditorCameraPosition() const;
    glm::vec3 getCreationPositionAt(const glm::vec2& viewportPosition) const;

    void update(float dt) override;
    void draw() override;

    // Вызывается Application раз в кадр после обновления сцены.
    void collectViews(WorldScene& scene, std::vector<ViewDesc>& out, const EditorViewContext& context = {});

    EditorViewport& viewport() { return m_viewport; }
    bool isHovered() const { return m_isHovered; }
    bool isPlayViewport() const { return m_isPlayViewport; }

private:
    // Отпускает камеру (пустой ввод) и приводит владение мышью в соответствие.
    // onlyIfCapturing: обновлять камеру, только если она сейчас летит.
    void idleCamera(float dt, bool onlyIfCapturing);

    void updateLevelEdit(WorldScene& scene, LevelEditSession& edit, bool hoverInput,
                         const Input::State& physicalInput);

    EditorViewport m_viewport;
    Entity* m_entity = nullptr;

    // Эта панель "держит" запущенную игру: показывает игровую камеру и отдаёт ей ввод.
    bool m_isPlayViewport = false;

    bool m_windowOpen = true;     // результат ImGui::Begin (false: свёрнута / вкладка не видна)
    bool m_focused = false;
    bool m_isHovered = false;

    ViewportImageRect m_image;    // где в экранных координатах нарисован вид

    ViewportMouseOwnership m_mouseOwnership;
    RightMouseGesture m_rightMouse;
    bool m_prevRightMouse = false;

    bool m_openCreatePopup = false;
    bool m_pendingTexturePaint = false;
    glm::vec2 m_contextClickPosition{0.0f};   // клик ПКМ в координатах изображения
    glm::vec2 m_contextPopupPosition{0.0f};   // клик ПКМ в экранных координатах
};
