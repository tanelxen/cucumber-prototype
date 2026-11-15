//
//  ViewportPanel.cpp
//  Cucumber
//

#include "ViewportPanel.h"

#include "GameViewProducer.h"
#include "RenderTarget.h"

#include "SceneManager.h"
#include "World.h"
#include "Entity.h"
#include "UIManager.h"
#include "MapComponent.h"

#include "level/LevelEditSession.h"
#include "level/LevelEditOverlay.h"
#include "level/FaceTexturing.h"

#include "LevelEditInput.h"
#include "ViewportAssetDrop.h"
#include "ViewportGizmo.h"
#include "ViewportPicking.h"
#include "ViewportToolbar.h"

#include <imgui.h>

namespace
{
    LevelEditSession* ActiveEditSession(WorldScene& scene)
    {
        auto* map = scene.getMap();
        return map ? map->editSession() : nullptr;
    }
}

ViewportPanel::ViewportPanel(const std::string& name, uint32_t id)
    : EditorPanel(name), m_viewport(id)
{
}

void ViewportPanel::setEntity(Entity* entity)
{
    m_entity = entity;
}

void ViewportPanel::setEditorCameraPosition(const glm::vec3& position)
{
    m_viewport.camera().position = position;
}

glm::vec3 ViewportPanel::getEditorCameraPosition() const
{
    return m_viewport.camera().position;
}

glm::vec3 ViewportPanel::getCreationPositionAt(const glm::vec2& viewportPosition) const
{
    const glm::ivec2 size = m_viewport.size();
    return ViewportPicking::CreationPosition(m_viewport.camera(), m_viewport.target(), size, viewportPosition);
}

void ViewportPanel::idleCamera(float dt, bool onlyIfCapturing)
{
    EditorCamera& camera = m_viewport.camera();
    if (!onlyIfCapturing || camera.isCapturingMouse())
    {
        camera.update(dt, {}, 0.0f, 1, false);   // отпускаем захват
    }
    m_mouseOwnership.sync(camera.isCapturingMouse());
}

void ViewportPanel::update(float dt)
{
    m_viewport.applyPendingSize();

    auto scene = SceneManager::Get().getActiveScene();
    const bool simulating = scene && scene->isSimulationMode();

    // Симуляцию могли остановить откуда угодно (другая панель, меню).
    if (m_isPlayViewport && !simulating)
    {
        m_isPlayViewport = false;
        Input::setGameInputState({});
    }

    EditorCamera& camera = m_viewport.camera();
    const Input::State physicalInput = Input::getState();
    const bool rightMouseDown = physicalInput.isRightMouseButtonPressed();
    const bool rightMousePressed = rightMouseDown && !m_prevRightMouse;
    const bool rightMouseReleased = !rightMouseDown && m_prevRightMouse;
    m_prevRightMouse = rightMouseDown;

    if (!isVisible || !m_windowOpen)
    {
        m_image.valid = false;
        m_rightMouse.cancel();
        if (m_isPlayViewport) Input::setGameInputState({});
        idleCamera(dt, true);
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 mouse = io.MousePos;
    const bool mouseInImage = m_isHovered && m_image.contains(mouse.x, mouse.y);

    const bool cursorLocked = !Input::isCursorEnabled();
    const bool iOwn = m_mouseOwnership.owns();
    const bool mouseFree = ViewportMouseOwnership::IsFree();

    const bool hoverInput = mouseFree && !cursorLocked && mouseInImage;   // обычное наведение
    const bool rightMouseNavigation = m_rightMouse.tracking() && m_rightMouse.navigating() && rightMouseDown;
    const bool editorInput = iOwn || hoverInput || rightMouseNavigation;
    const bool gameCursorCaptured = m_isPlayViewport && mouseFree && cursorLocked;

    const Input::State panelInput = (editorInput || gameCursorCaptured) ? physicalInput : Input::State{};

    if (m_isPlayViewport)
    {
        Input::setGameInputState(panelInput);
    }

    if (!scene || (m_isPlayViewport && scene->hasActiveSceneCamera()))
    {
        m_rightMouse.cancel();
        idleCamera(dt, false);
        return;
    }

    // --- Жест ПКМ: короткий клик (меню создания / перенос текстуры) или навигация камерой ---
    if (rightMousePressed && hoverInput)
    {
        m_rightMouse.press(io.KeyAlt);
        m_contextClickPosition = m_image.toLocal(mouse.x, mouse.y);
        m_contextPopupPosition = { mouse.x, mouse.y };
    }

    if (m_rightMouse.tracking() && rightMouseDown && !m_rightMouse.navigating())
    {
        m_rightMouse.accumulate(physicalInput.getMouseOffsetX(), physicalInput.getMouseOffsetY());
    }

    if (rightMouseReleased && m_rightMouse.tracking())
    {
        switch (m_rightMouse.release(!scene->isSimulationMode()))
        {
            case RightMouseGesture::Release::AltClick:
                // Alt+ПКМ при правке геометрии: перенос текстурирования (как в Hammer), меню создания не открываем
                if (ActiveEditSession(*scene)) m_pendingTexturePaint = true;
                else                           m_openCreatePopup = true;
                break;
            case RightMouseGesture::Release::Click:
                m_openCreatePopup = true;
                break;
            case RightMouseGesture::Release::None:
                break;
        }
    }

    // --- Камера ---
    if (editorInput)
    {
        const float wheel = hoverInput ? io.MouseWheel : 0.0f;
        camera.update(dt, panelInput, wheel, m_viewport.size().y, rightMouseNavigation);
    }
    else if (camera.isCapturingMouse())
    {
        camera.update(dt, {}, 0.0f, 1, false);
    }
    m_mouseOwnership.sync(camera.isCapturingMouse());

    if (auto* edit = ActiveEditSession(*scene))
    {
        updateLevelEdit(*scene, *edit, hoverInput, physicalInput);
    }

    m_pendingTexturePaint = false;
}

void ViewportPanel::updateLevelEdit(WorldScene& scene, LevelEditSession& edit, bool hoverInput,
                                    const Input::State& physicalInput)
{
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const ViewportPointer pointer = LevelEditInput::MakePointer(m_viewport, m_image, { mouse.x, mouse.y },
                                                                hoverInput, physicalInput);
    edit.handlePointer(m_viewport.id(), pointer);

    if (m_pendingTexturePaint)
    {
        m_pendingTexturePaint = false;
        FaceTexturing::PaintHoveredFace(edit);
    }

    if (m_focused && !pointer.capturing)
    {
        const LevelEditInput::ShortcutResult shortcuts = LevelEditInput::HandleShortcuts(edit);
        if (shortcuts.saveRequested)
        {
            if (auto* map = scene.getMap()) map->saveLevel();
        }
    }
}

void ViewportPanel::collectViews(WorldScene& scene, std::vector<ViewDesc>& out, const EditorViewContext& context)
{
    // Свёрнутые / скрытые панели вообще не рендерим.
    if (!isVisible || !m_windowOpen) return;

    const bool simulating = scene.isSimulationMode();

    if (simulating && m_isPlayViewport)
    {
        GameViewParams params;
        params.target = &m_viewport.target();
        params.size = m_viewport.size();
        params.baseViewId = m_viewport.id() * 64;
        params.drawSkybox = m_viewport.drawSkybox;

        if (ProduceGameViews(scene.cameras(), params, out)) return;
        // В сцене нет игровой камеры — показываем редакторскую.
    }

    m_viewport.drawEditOverlay = ActiveEditSession(scene) != nullptr;
    m_viewport.produceEditorView(out, context);
}

void ViewportPanel::draw()
{
    auto scene = SceneManager::Get().getActiveScene();
    if (!scene) return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
    m_windowOpen = ImGui::Begin(m_name.c_str());

    if (!m_windowOpen)
    {
        m_image.valid = false;
        m_focused = false;
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    m_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    EditorCamera& camera = m_viewport.camera();

    // --- Тулбар ---
    switch (DrawViewportToolbar(m_viewport, m_manager->gridSize, scene->getMap() != nullptr,
                                m_focused, scene->isSimulationMode()))
    {
        case ToolbarAction::Stop:
            m_isPlayViewport = false;
            Input::setGameInputState({});
            SceneManager::Get().stopSimulation();
            break;

        case ToolbarAction::Play:
            if (auto* map = scene->getMap()) map->endEdit();

            m_isPlayViewport = true;
            SceneManager::Get().startSimulation();
            break;

        case ToolbarAction::None:
            break;
    }

    // --- Размер: пока зажата ЛКМ (тянут разделитель окон) не реаллоцируем цель ---
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        m_viewport.requestSize(static_cast<int>(avail.x), static_cast<int>(avail.y),
                               ImGui::GetIO().DisplayFramebufferScale.x);
    }

    const ImVec2 contentPos = ImGui::GetCursorScreenPos();

    ImGui::Image((ImTextureID)(intptr_t)m_viewport.target().texture(),
                 avail,
                 ImVec2(0, 1),
                 ImVec2(1, 0));

    const ImVec2 imageMin = ImGui::GetItemRectMin();
    const ImVec2 imageMax = ImGui::GetItemRectMax();
    m_isHovered = ImGui::IsItemHovered();
    m_image = ViewportImageRect::FromMinMax(imageMin.x, imageMin.y, imageMax.x, imageMax.y);

    // Цель drag-and-drop привязывается к последнему элементу, поэтому обрабатываем её сразу после Image
    // (раньше она шла после подписей правки, и в режиме правки целью оказывался текст подсказки).
    if (!scene->isSimulationMode())
    {
        const auto positionAt = [this](const glm::vec2& position) { return getCreationPositionAt(position); };
        if (Entity* dropped = ViewportAssetDrop::Handle(*scene, m_image, positionAt, m_manager->gridSize))
        {
            m_manager->setEntity(dropped);
        }
    }

    if (auto* edit = ActiveEditSession(*scene))
    {
        DrawLevelEditLabels(*edit, camera.build(m_viewport.aspect()),
                            m_image.left, m_image.top, m_image.right - m_image.left, m_image.bottom - m_image.top);

        ImGui::SetCursorScreenPos(ImVec2(m_image.left + 8, m_image.top + 8));
        ImGui::TextColored(ImVec4(1, 1, 1, 0.85f), "%s: %s", edit->activeTool().name(), edit->activeTool().hint().c_str());
    }

    // --- Меню создания по ПКМ ---
    if (m_openCreatePopup)
    {
        ImGui::SetNextWindowPos(ImVec2(m_contextPopupPosition.x, m_contextPopupPosition.y));
        ImGui::OpenPopup("ViewportCreatePopup");
        m_openCreatePopup = false;
    }

    if (ImGui::BeginPopup("ViewportCreatePopup"))
    {
        const glm::vec2 clickPosition = m_image.clampLocal(m_contextClickPosition);
        m_manager->drawEntityCreationMenu(*scene, getCreationPositionAt(clickPosition));
        ImGui::EndPopup();
    }

    if (camera.isCapturingMouse())
    {
        ImGui::SetWindowFocus();
    }

    if (!scene->isSimulationMode() && !ActiveEditSession(*scene))
    {
        const bool gizmoEnabled = ViewportMouseOwnership::IsFree() && Input::isCursorEnabled();
        ViewportGizmo::Draw(m_entity, camera.build(m_viewport.aspect()), contentPos, avail,
                            static_cast<int>(m_viewport.id()), gizmoEnabled, m_manager->gridSize);
    }

    ImGui::End();
    ImGui::PopStyleVar();
}
