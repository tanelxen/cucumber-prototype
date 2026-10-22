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
#include "Input.h"

#include "../vendor/IconsMaterialDesignIcons.h"

#include <imgui.h>
#include <ImGuizmo.h>

static bool DrawPlayButton();
static bool DrawStopButton();

namespace
{
    ViewportPanel* s_mouseOwner = nullptr;   // единственная панель, которой сейчас принадлежит мышь
}

static void DrawGizmo(Entity* selectedNode, const CameraData& camera, const ImVec2& contentPos,
                      const ImVec2& contentSize, int id, bool enabled);

ViewportPanel::ViewportPanel(const std::string& name, uint32_t id)
    : EditorPanel(name), m_viewport(id)
{
}

ViewportPanel::~ViewportPanel()
{
    if (s_mouseOwner == this)
    {
        s_mouseOwner = nullptr;
    }
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

void ViewportPanel::syncCursor()
{
    // Курсором управляем только по смене состояния — иначе несколько панелей и игра дерутся за него.
    const bool capturing = m_viewport.camera().isCapturingMouse();
    if (capturing == m_cursorCaptured) return;

    m_cursorCaptured = capturing;
    Input::setCursorEnabled(!capturing);
}

void ViewportPanel::updateOwnership()
{
    const bool capturing = m_viewport.camera().isCapturingMouse();
    
    if (capturing && !s_mouseOwner)              s_mouseOwner = this;
    else if (!capturing && s_mouseOwner == this) s_mouseOwner = nullptr;
    
    syncCursor();
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
    
    // Фронт нажатия ПКМ — считаем всегда, даже если панель скрыта, чтобы состояние не устаревало.
    const bool rmbDown = physicalInput.isRightMouseButtonPressed();
    const bool rmbPressedNow = rmbDown && !m_prevRightMouse;
    m_prevRightMouse = rmbDown;
    
    if (!isVisible || !m_windowOpen)
    {
        m_hasImageBounds = false;
        if (m_isPlayViewport) Input::setGameInputState({});
        if (camera.isCapturingMouse())
        {
            camera.update(dt, {}, 0.0f, 1, false);   // отпускаем захват
            updateOwnership();
        }
        return;
    }
    
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool mouseInImage = m_hasImageBounds && m_isHovered &&
    mouse.x >= m_imageLeft && mouse.x < m_imageRight &&
    mouse.y >= m_imageTop && mouse.y < m_imageBottom;
    
    const bool cursorLocked = !Input::isCursorEnabled();
    const bool iOwn = (s_mouseOwner == this);
    const bool mouseFree = (s_mouseOwner == nullptr);
    
    const bool hoverInput = mouseFree && !cursorLocked && mouseInImage;   // обычное наведение
    const bool editorInput = iOwn || hoverInput;
    const bool gameCursorCaptured = m_isPlayViewport && mouseFree && cursorLocked;
    
    const Input::State panelInput = (editorInput || gameCursorCaptured) ? physicalInput : Input::State{};
    
    if (m_isPlayViewport)
    {
        Input::setGameInputState(panelInput);
    }
    
    if (!scene) return;
    
    if (m_isPlayViewport && scene->hasActiveSceneCamera()) return;
    
    if (editorInput)
    {
        const bool canStartCapture = hoverInput && rmbPressedNow;
        const float wheel = hoverInput ? ImGui::GetIO().MouseWheel : 0.0f;
        
        camera.update(dt, panelInput, wheel, m_viewport.size().y, canStartCapture);
        updateOwnership();
    }
}

void ViewportPanel::collectViews(WorldScene& scene, std::vector<ViewDesc>& out)
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
        // В сцене нет игровой камеры — показываем редакторскую (как раньше).
    }

    const bool audioListener = simulating ? m_isPlayViewport : m_focused;
    m_viewport.produceEditorView(out, audioListener);
}

void ViewportPanel::draw()
{
    auto scene = SceneManager::Get().getActiveScene();
    if (!scene) return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
    m_windowOpen = ImGui::Begin(m_name.c_str());

    if (!m_windowOpen)
    {
        m_hasImageBounds = false;
        m_focused = false;
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    m_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    EditorCamera& camera = m_viewport.camera();

    // --- Тулбар ---
    ImGui::Checkbox("Grid", &m_viewport.drawGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Gizmos", &m_viewport.drawGizmos);
    ImGui::SameLine();
    ImGui::Checkbox("Sky", &m_viewport.drawSkybox);
    ImGui::SameLine();

    int mode = static_cast<int>(camera.mode());
    ImGui::SetNextItemWidth(110);
    if (ImGui::Combo("##ViewMode", &mode, "Perspective\0Top\0Front\0Side\0"))
    {
        camera.setMode(static_cast<EditorViewMode>(mode));
    }

    ImGui::SameLine(ImGui::GetWindowWidth() - 48);

    if (scene->isSimulationMode())
    {
        if (DrawStopButton())
        {
            m_isPlayViewport = false;
            Input::setGameInputState({});
            SceneManager::Get().stopSimulation();
        }
    }
    else if (DrawPlayButton())
    {
        m_isPlayViewport = true;
        SceneManager::Get().startSimulation();
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

    const ImVec2 viewportMin = ImGui::GetItemRectMin();
    const ImVec2 viewportMax = ImGui::GetItemRectMax();
    m_isHovered = ImGui::IsItemHovered();
    m_imageLeft = viewportMin.x;
    m_imageTop = viewportMin.y;
    m_imageRight = viewportMax.x;
    m_imageBottom = viewportMax.y;
    m_hasImageBounds = viewportMax.x > viewportMin.x && viewportMax.y > viewportMin.y;

    if (camera.isCapturingMouse())
    {
        ImGui::SetWindowFocus();
    }
    
    if (!scene->isSimulationMode())
    {
        const bool gizmoEnabled = (s_mouseOwner == nullptr) && Input::isCursorEnabled();
        DrawGizmo(m_entity, camera.build(m_viewport.aspect()), contentPos, avail,
                  static_cast<int>(m_viewport.id()), gizmoEnabled);
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

static bool DrawPlayButton()
{
    ImVec4 bgColor          = ImVec4(0.08f, 0.25f, 0.14f, 1.0f);
    ImVec4 bgHoveredColor   = ImVec4(0.12f, 0.33f, 0.18f, 1.0f);
    ImVec4 bgActiveColor    = ImVec4(0.05f, 0.18f, 0.10f, 1.0f);
    ImVec4 borderColor      = ImVec4(0.12f, 0.33f, 0.18f, 1.0f);
    ImVec4 textColor        = ImVec4(0.85f, 0.95f, 0.88f, 1.0f);
    
    // Цветовой код для ручной отрисовки (ImColor принимает ImVec4)
    ImColor iconColor       = ImColor(ImVec4(0.30f, 0.85f, 0.45f, 1.0f));
    
    ImGui::PushStyleColor(ImGuiCol_Button, bgColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bgHoveredColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, bgActiveColor);
    ImGui::PushStyleColor(ImGuiCol_Border, borderColor);
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    
    // Оставляем место слева под иконку с помощью пробелов перед словом
    bool pressed = ImGui::Button("##ViewportPlay", ImVec2(32, 32));
    
    // Получаем координаты только что отрисованной кнопки
    ImVec2 min_p = ImGui::GetItemRectMin();
    ImVec2 max_p = ImGui::GetItemRectMax();
    
    // Вычисляем позицию для треугольника Play (внутри кнопки слева)
    float button_height = max_p.y - min_p.y;
    float button_width = max_p.x - min_p.x;
    float icon_size = 12.0f; // Размер иконки в пикселях
    float icon_left = min_p.x + (button_width - icon_size) * 0.5f + 1; // Отступ слева
    float icon_top = min_p.y + (button_height - icon_size) * 0.5f;
    
    // Координаты трех вершин треугольника
    ImVec2 p1 = ImVec2(icon_left, icon_top);
    ImVec2 p2 = ImVec2(icon_left, icon_top + icon_size);
    ImVec2 p3 = ImVec2(icon_left + icon_size * 0.85f, icon_top + icon_size * 0.5f);
    
    // Отрисовываем заполненный треугольник во внутренний слой окна
    ImGui::GetWindowDrawList()->AddTriangleFilled(p1, p2, p3, iconColor);
    
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(5);
    
    return pressed;
}

static bool DrawStopButton()
{
    ImVec4 bgColor          = ImVec4(0.28f, 0.08f, 0.08f, 1.0f); // Темно-красный фон
    ImVec4 bgHoveredColor   = ImVec4(0.38f, 0.12f, 0.12f, 1.0f); // Светлее при наведении
    ImVec4 bgActiveColor    = ImVec4(0.20f, 0.05f, 0.05f, 1.0f); // Темнее при нажатии
    ImVec4 borderColor      = ImVec4(0.38f, 0.12f, 0.12f, 1.0f); // Светло-розовый/белый текст
    ImVec4 textColor        = ImVec4(0.95f, 0.85f, 0.85f, 1.0f); // Светло-розовый/белый текст
    
    // Цветовой код для ручной отрисовки (ImColor принимает ImVec4)
    ImColor iconColor       = ImColor(ImVec4(0.85f, 0.30f, 0.30f, 1.0f)); // Ярко-красная иконка
    
    ImGui::PushStyleColor(ImGuiCol_Button, bgColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bgHoveredColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, bgActiveColor);
    ImGui::PushStyleColor(ImGuiCol_Border, borderColor);
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    
    // Оставляем место слева под иконку с помощью пробелов перед словом
    bool pressed = ImGui::Button("##ViewportStop", ImVec2(32, 32));
    
    // Получаем координаты только что отрисованной кнопки
    ImVec2 min_p = ImGui::GetItemRectMin();
    ImVec2 max_p = ImGui::GetItemRectMax();
    
    // Вычисляем позицию для треугольника Play (внутри кнопки слева)
    float button_height = max_p.y - min_p.y;
    float button_width = max_p.x - min_p.x;
    float icon_size = 12.0f; // Размер иконки в пикселях
    float icon_left = min_p.x + (button_width - icon_size) * 0.5f; // Отступ слева
    float icon_top = min_p.y + (button_height - icon_size) * 0.5f;
    
    // Вычисляем углы квадрата Stop
    ImVec2 rectMin = ImVec2(icon_left, icon_top);
    ImVec2 rectMax = ImVec2(icon_left + icon_size, icon_top + icon_size);
    
    // Рисуем заполненный квадрат Stop (с небольшим скруглением углов для красоты)
    ImGui::GetWindowDrawList()->AddRectFilled(rectMin, rectMax, iconColor, 2.0f);
    
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(5);
    
    return pressed;
}

static void DrawGizmo(Entity* selectedNode, const CameraData& camera, const ImVec2& contentPos,
                      const ImVec2& contentSize, int id, bool enabled)
{
    ImGuizmo::AllowAxisFlip(false);

    if (!selectedNode) return;

    ImGuizmo::SetOrthographic(camera.ortho);
    ImGuizmo::BeginFrame();
    ImGuizmo::Enable(enabled);
    ImGuizmo::SetDrawlist();
    
    ImGuizmo::PushID(id);
    ImGuizmo::SetRect(contentPos.x, contentPos.y, contentSize.x, contentSize.y);

    auto& transform = selectedNode->getTransform();
    glm::mat4 worldMatrix = transform.getWorldMatrix();

    const float* viewPtr = &camera.view[0][0];
    const float* projPtr = &camera.proj[0][0];
    float* matrixPtr = &worldMatrix[0][0];

    float snap[3] = { 8.f, 8.f, 8.f };
    ImGuizmo::Manipulate(viewPtr, projPtr, ImGuizmo::TRANSLATE, ImGuizmo::LOCAL, matrixPtr, nullptr, snap);

    if (ImGuizmo::IsUsing())
    {
        transform.setWorldMatrix(worldMatrix);
    }
    
    ImGuizmo::PopID();
}
