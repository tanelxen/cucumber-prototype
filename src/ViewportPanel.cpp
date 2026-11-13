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
#include "UIManager.h"
#include "AssetsPanel.h"
#include "MeshComponent.h"
#include "ModelLoader.h"
#include "ProjectManager.h"
#include "SceneSerializer.h"

#include "level/LevelEditSession.h"
#include "level/LevelEditOverlay.h"
#include "level/FaceTexturing.h"
#include "MapComponent.h"

#include "../vendor/IconsMaterialDesignIcons.h"

#include <imgui.h>
#include <ImGuizmo.h>

#include <algorithm>
#include <cmath>
#include <iostream>

static bool DrawPlayButton();
static bool DrawStopButton();

namespace
{
    ViewportPanel* s_mouseOwner = nullptr;   // единственная панель, которой сейчас принадлежит мышь

    bool ReadWorldPosition(const glm::mat4& inverseViewProj, const RenderTarget& target,
                           int pixelX, int pixelYFromTop, glm::vec3& position)
    {
        pixelX = std::clamp(pixelX, 0, target.pixelWidth() - 1);
        pixelYFromTop = std::clamp(pixelYFromTop, 0, target.pixelHeight() - 1);
        const int pixelY = target.pixelHeight() - 1 - pixelYFromTop;

        float depth = 1.0f;
        if (!target.readDepth(pixelX, pixelY, depth) || depth >= 1.0f ||
            !std::isfinite(depth)) {
            return false;
        }

        const float x = 2.0f * (static_cast<float>(pixelX) + 0.5f) /
                        static_cast<float>(target.pixelWidth()) - 1.0f;
        const float y = 2.0f * (static_cast<float>(pixelY) + 0.5f) /
                        static_cast<float>(target.pixelHeight()) - 1.0f;
        const float z = depth * 2.0f - 1.0f;
        const glm::vec4 world = inverseViewProj * glm::vec4(x, y, z, 1.0f);
        if (std::abs(world.w) <= 0.0001f) return false;

        position = glm::vec3(world) / world.w;
        return true;
    }

    bool GetSurfaceAt(const EditorCamera& camera, const RenderTarget& target,
                      const glm::ivec2& viewportSize, const glm::vec2& mousePosition,
                      glm::vec3& position, glm::vec3& normal)
    {
        normal = glm::vec3(0.0f);
        const CameraData cameraData = camera.build(
            static_cast<float>(viewportSize.x) / static_cast<float>(viewportSize.y));
        const glm::mat4 inverseViewProj = glm::inverse(cameraData.viewProj());
        const int pixelX = static_cast<int>(mousePosition.x * target.pixelWidth() / viewportSize.x);
        const int pixelY = static_cast<int>(mousePosition.y * target.pixelHeight() / viewportSize.y);
        if (!ReadWorldPosition(inverseViewProj, target, pixelX, pixelY, position)) return false;

        glm::vec3 left, right, up, down;
        if (!ReadWorldPosition(inverseViewProj, target, pixelX - 1, pixelY, left) ||
            !ReadWorldPosition(inverseViewProj, target, pixelX + 1, pixelY, right) ||
            !ReadWorldPosition(inverseViewProj, target, pixelX, pixelY - 1, up) ||
            !ReadWorldPosition(inverseViewProj, target, pixelX, pixelY + 1, down)) return true;

        const glm::vec3 horizontal = right - left;
        const glm::vec3 vertical = down - up;
        const glm::vec3 cross = glm::cross(horizontal, vertical);
        if (glm::dot(cross, cross) < 1e-8f) return true;

        const float centerDepth = -glm::vec3(cameraData.view * glm::vec4(position, 1.0f)).z;
        const float edgeDepth = std::max({
            std::abs(-glm::vec3(cameraData.view * glm::vec4(left, 1.0f)).z - centerDepth),
            std::abs(-glm::vec3(cameraData.view * glm::vec4(right, 1.0f)).z - centerDepth),
            std::abs(-glm::vec3(cameraData.view * glm::vec4(up, 1.0f)).z - centerDepth),
            std::abs(-glm::vec3(cameraData.view * glm::vec4(down, 1.0f)).z - centerDepth)
        });
        if (edgeDepth > std::max(8.0f, std::abs(centerDepth) * 0.1f)) return true;

        normal = glm::normalize(cross);
        if (glm::dot(normal, -cameraData.forward) < 0.0f) {
            normal = -normal;
        }
        return true;
    }

    glm::vec3 GetCreationPosition(const EditorCamera& camera, const RenderTarget& target,
                                  const glm::ivec2& viewportSize, const glm::vec2& mousePosition)
    {
        glm::vec3 position;
        glm::vec3 normal;
        if (GetSurfaceAt(camera, target, viewportSize, mousePosition, position, normal)) {
            return position;
        }

        const CameraData cameraData = camera.build(
            static_cast<float>(viewportSize.x) / static_cast<float>(viewportSize.y));

        glm::vec3 rayStart;
        glm::vec3 rayDirection;
        ScreenPointToRay(cameraData, viewportSize, mousePosition, rayStart, rayDirection);

        if (!cameraData.ortho) return cameraData.position + rayDirection * 64.0f;

        const float forwardDistance = glm::dot(cameraData.forward,
            cameraData.position + cameraData.forward * 64.0f - rayStart);
        const float rayForward = glm::dot(rayDirection, cameraData.forward);
        return rayStart + rayDirection * (forwardDistance / rayForward);
    }
    
    glm::vec3 SnapPivotToGrid(const glm::vec3& position, float step)
    {
        return glm::round(position / step) * step;
    }

    void CreateMeshEntity(WorldScene& scene, const AssetDragPayload& payload,
                          const glm::vec3& position)
    {
        const char* pathEnd = std::find(payload.path, payload.path + sizeof(payload.path), '\0');
        if (pathEnd == payload.path + sizeof(payload.path))
        {
            std::cerr << "[Viewport] Cannot drop mesh: asset path is not terminated.\n";
            return;
        }

        const std::filesystem::path assetPath(payload.path);
        const ModelResource resource = ModelLoader::LoadModel(assetPath.string());
        if (!resource.mesh)
        {
            std::cerr << "[Viewport] Failed to load mesh asset: " << assetPath << '\n';
            return;
        }

        auto entity = std::make_unique<Entity>("Mesh_" + assetPath.stem().string());
        entity->scene = &scene;
        entity->getTransform().setPosition(position);

        Entity* createdEntity = entity.get();
        scene.m_entities.push_back(std::move(entity));
        auto* mesh = createdEntity->addComponent<MeshComponent>();
        if (!mesh->loadModel(assetPath))
        {
            std::cerr << "[Viewport] Failed to initialize mesh asset: " << assetPath << '\n';
            createdEntity->destroy();
            return;
        }

        createdEntity->getTransform().setPosition(SnapPivotToGrid(position, g_UIManager->gridSize));

        g_UIManager->setEntity(createdEntity);
    }

    void InstantiatePrefabAt(WorldScene& scene, const AssetDragPayload& payload,
                             const glm::vec3& position)
    {
        const char* pathEnd = std::find(payload.path, payload.path + sizeof(payload.path), '\0');
        if (pathEnd == payload.path + sizeof(payload.path))
        {
            std::cerr << "[Viewport] Cannot drop prefab: asset path is not terminated.\n";
            return;
        }

        const std::filesystem::path assetPath(payload.path);
        Entity* root = SceneSerializer::InstantiatePrefab(
            &scene, assetPath.string(), nullptr, UID::Invalid(), false);
        if (!root)
        {
            std::cerr << "[Viewport] Failed to instantiate prefab: " << assetPath << '\n';
            return;
        }

        root->getTransform().setPosition(SnapPivotToGrid(position, g_UIManager->gridSize));
        SceneSerializer::InitializePrefabComponents(root);
        g_UIManager->setEntity(root);
    }
    
    static LevelEditSession* ActiveEditSession(WorldScene& scene)
    {
        auto* map = scene.getMap();
        return map ? map->editSession() : nullptr;
    }
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

glm::vec3 ViewportPanel::getCreationPositionAt(const glm::vec2& viewportPosition) const
{
    const glm::ivec2 size = m_viewport.size();
    return GetCreationPosition(m_viewport.camera(), m_viewport.target(), size, viewportPosition);
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
    const bool rightMouseDown = physicalInput.isRightMouseButtonPressed();
    const bool rightMousePressed = rightMouseDown && !m_prevRightMouse;
    const bool rightMouseReleased = !rightMouseDown && m_prevRightMouse;
    m_prevRightMouse = rightMouseDown;
    
    if (!isVisible || !m_windowOpen)
    {
        m_hasImageBounds = false;
        m_rightMouseTracking = false;
        m_rightMouseNavigating = false;
        if (m_isPlayViewport) Input::setGameInputState({});
        if (camera.isCapturingMouse())
        {
            camera.update(dt, {}, 0.0f, 1, false);   // отпускаем захват
        }
        updateOwnership();
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
    const bool rightMouseNavigation = m_rightMouseTracking &&
                                     m_rightMouseNavigating && rightMouseDown;
    const bool editorInput = iOwn || hoverInput || rightMouseNavigation;
    const bool gameCursorCaptured = m_isPlayViewport && mouseFree && cursorLocked;
    
    const Input::State panelInput = (editorInput || gameCursorCaptured) ? physicalInput : Input::State{};
    
    if (m_isPlayViewport)
    {
        Input::setGameInputState(panelInput);
    }
    
    if (!scene || (m_isPlayViewport && scene->hasActiveSceneCamera()))
    {
        m_rightMouseTracking = false;
        m_rightMouseNavigating = false;
        camera.update(dt, {}, 0.0f, 1, false);
        updateOwnership();
        return;
    }

    if (rightMousePressed && hoverInput)
    {
        m_rightMouseTracking = true;
        m_rightMouseNavigating = false;
        m_rightMouseTravel = 0.0f;
        m_rightMouseAlt = ImGui::GetIO().KeyAlt;
        m_contextClickPosition = {mouse.x - m_imageLeft, mouse.y - m_imageTop};
        m_contextPopupPosition = {mouse.x, mouse.y};
    }

    if (m_rightMouseTracking && rightMouseDown && !m_rightMouseNavigating)
    {
        m_rightMouseTravel += std::sqrt(
            physicalInput.getMouseOffsetX() * physicalInput.getMouseOffsetX() +
            physicalInput.getMouseOffsetY() * physicalInput.getMouseOffsetY());
        m_rightMouseNavigating = m_rightMouseTravel >= 5.0f;
    }

    if (rightMouseReleased && m_rightMouseTracking)
    {
        const bool click = !m_rightMouseNavigating && !scene->isSimulationMode();
        
        // Alt+ПКМ при правке геометрии: перенос текстурирования (как в Hammer), меню создания не открываем
        if (click && m_rightMouseAlt && ActiveEditSession(*scene))
        {
            m_pendingTexturePaint = true;
        }
        else
        {
            m_openCreatePopup = click;
        }
        
        m_rightMouseTracking = false;
        m_rightMouseNavigating = false;
    }

    if (editorInput)
    {
        const float wheel = hoverInput ? ImGui::GetIO().MouseWheel : 0.0f;
        camera.update(dt, panelInput, wheel, m_viewport.size().y,
                      rightMouseNavigation);
    }
    else if (camera.isCapturingMouse())
    {
        camera.update(dt, {}, 0.0f, 1, false);
    }
    updateOwnership();
    
    if (auto* edit = ActiveEditSession(*scene))
    {
        ImGuiIO& io = ImGui::GetIO();
        const bool mod = io.KeyCtrl || io.KeySuper;   // Cmd на macOS
        
        ViewportPointer pointer;
        pointer.camera   = camera.build(m_viewport.aspect());
        pointer.sizePx   = { int(m_imageRight - m_imageLeft), int(m_imageBottom - m_imageTop) };
        pointer.mousePx  = { mouse.x - m_imageLeft, mouse.y - m_imageTop };
        pointer.over     = hoverInput && !camera.isCapturingMouse();
        pointer.down     = physicalInput.isLeftMouseButtonPressed();
        pointer.pressed  = physicalInput.isLeftMouseButtonJustPressed();
        pointer.released = physicalInput.isLeftMouseButtonClicked();
        pointer.ctrl = mod;
        pointer.shift = io.KeyShift;
        pointer.alt = io.KeyAlt;
        pointer.capturing = camera.isCapturingMouse();
        edit->handlePointer(m_viewport.id(), pointer);
        
        if (m_pendingTexturePaint)
        {
            m_pendingTexturePaint = false;
            FaceTexturing::PaintHoveredFace(*edit);
        }
        
        if (m_focused && !io.WantTextInput && !pointer.capturing)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_B, false)) edit->setTool(ToolType::Block);
            if (ImGui::IsKeyPressed(ImGuiKey_V, false)) edit->setTool(ToolType::Select);
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) edit->escape();
            
            if (mod && ImGui::IsKeyPressed(ImGuiKey_Z, false)) { if (io.KeyShift) edit->redo(); else edit->undo(); }
            if (mod && ImGui::IsKeyPressed(ImGuiKey_Y, false)) edit->redo();
            if (mod && ImGui::IsKeyPressed(ImGuiKey_S, false)) scene->getMap()->saveLevel();
            if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false))
                edit->deleteSelection();
            
            const bool plain = !mod;   // Ctrl+Z / Ctrl+Y остаются за undo / redo
            if (ImGui::IsKeyPressed(ImGuiKey_E, false)) edit->setTool(ToolType::Vertex);
            if (plain)
            {
                if (ImGui::IsKeyPressed(ImGuiKey_X, false)) edit->handleKey(LevelKey::X, io.KeyShift);
                if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) edit->handleKey(LevelKey::Y, io.KeyShift);
                if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) edit->handleKey(LevelKey::Z, io.KeyShift);
                if (ImGui::IsKeyPressed(ImGuiKey_G, false)) edit->handleKey(LevelKey::G, false);
                if (ImGui::IsKeyPressed(ImGuiKey_A, false)) edit->handleKey(LevelKey::SelectAll, false);
                if (ImGui::IsKeyPressed(ImGuiKey_1, false)) edit->handleKey(LevelKey::Num1, false);
                if (ImGui::IsKeyPressed(ImGuiKey_2, false)) edit->handleKey(LevelKey::Num2, false);
                if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))
                    edit->handleKey(LevelKey::Enter, false);
            }
            
            if (mod)
            {
                if (ImGui::IsKeyPressed(ImGuiKey_C, false)) edit->handleKey(LevelKey::Copy, false);
                if (ImGui::IsKeyPressed(ImGuiKey_X, false)) edit->handleKey(LevelKey::Cut, false);
                if (ImGui::IsKeyPressed(ImGuiKey_V, false)) edit->handleKey(LevelKey::Paste, false);
                if (ImGui::IsKeyPressed(ImGuiKey_D, false)) edit->handleKey(LevelKey::Duplicate, false);
            }
            else if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_D, false))
            {
                edit->handleKey(LevelKey::DuplicateMove, true);
            }
        }
    }
    
    m_pendingTexturePaint = false;
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
        // В сцене нет игровой камеры — показываем редакторскую (как раньше).
    }

    const bool audioListener = simulating ? m_isPlayViewport : m_focused;
    
    m_viewport.drawEditOverlay = ActiveEditSession(scene) != nullptr;
    m_viewport.produceEditorView(out, audioListener, context);
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

    int mode = static_cast<int>(camera.mode());
    ImGui::SetNextItemWidth(110);
    if (ImGui::Combo("##ViewMode", &mode, "Perspective\0Top\0Front\0Side\0"))
    {
        camera.setMode(static_cast<EditorViewMode>(mode));
        
        // Как в TrenchBroom / Hammer: ортографика по умолчанию каркасная, перспектива закрашенная
        // (0 = Perspective, порядок как в комбо). Строку можно удалить, если режим должен быть независим от проекции.
        m_viewport.shadeMode = (mode == 0) ? ShadeMode::Shaded : ShadeMode::Wireframe;
    }
    
    ImGui::SameLine();
    int shading = static_cast<int>(m_viewport.shadeMode);
    ImGui::SetNextItemWidth(100);
    if (ImGui::Combo("##ShadeMode", &shading, "Shaded\0Wireframe\0"))
    {
        m_viewport.shadeMode = static_cast<ShadeMode>(shading);
    }
    
    if (auto* map = scene->getMap())
    {
        ImGui::SameLine();
        char preview[16];
        std::snprintf(preview, sizeof(preview), "%g", g_UIManager->gridSize);
        
        ImGui::SetNextItemWidth(64);
        if (ImGui::BeginCombo("##GridSize", preview))
        {
            for (float step : { 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 64.0f, 128.0f, 256.0f })
            {
                char label[16];
                std::snprintf(label, sizeof(label), "%g", step);
                if (ImGui::Selectable(label, step == g_UIManager->gridSize)) g_UIManager->gridSize = step;
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Grid size  ( [ / ] )");
        
        // [ и ]: шаг вдвое меньше / больше. Обрабатывает только панель в фокусе, иначе при нескольких панелях
        // клавиша сработала бы дважды.
        if (m_focused && !ImGui::GetIO().WantTextInput)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket, false)) g_UIManager->gridSize = std::max(1.0f, g_UIManager->gridSize * 0.5f);
            if (ImGui::IsKeyPressed(ImGuiKey_RightBracket, false)) g_UIManager->gridSize = std::min(1024.0f, g_UIManager->gridSize * 2.0f);
        }
    }

    ImGui::SameLine();
    ImGui::TextDisabled("RMB: Fly / Pan");

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
        if (auto* map = scene->getMap()) map->endEdit();
        
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
    
    if (auto* edit = ActiveEditSession(*scene))
    {
        DrawLevelEditLabels(*edit, camera.build(m_viewport.aspect()),
                            m_imageLeft, m_imageTop, m_imageRight - m_imageLeft, m_imageBottom - m_imageTop);
        
        ImGui::SetCursorScreenPos(ImVec2(m_imageLeft + 8, m_imageTop + 8));
        ImGui::TextColored(ImVec4(1, 1, 1, 0.85f), "%s: %s", edit->activeTool().name(), edit->activeTool().hint().c_str());
    }

    if (!scene->isSimulationMode() && ImGui::BeginDragDropTarget())
    {
        const ImGuiPayload* currentPayload = ImGui::GetDragDropPayload();
        const auto* currentAsset = currentPayload &&
            currentPayload->IsDataType("ASSET_ITEM") &&
            currentPayload->DataSize == sizeof(AssetDragPayload)
            ? static_cast<const AssetDragPayload*>(currentPayload->Data)
            : nullptr;
        if (currentAsset &&
            (currentAsset->type == AssetType_Mesh || currentAsset->type == AssetType_Prefab))
        {
            const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload("ASSET_ITEM", ImGuiDragDropFlags_AcceptBeforeDelivery);
            if (payload)
            {
                const ImVec2 mousePosition = ImGui::GetIO().MousePos;
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const bool isMesh = currentAsset->type == AssetType_Mesh;
                drawList->AddCircle(mousePosition, 9.0f, IM_COL32(255, 210, 80, 255), 20, 2.0f);
                drawList->AddText(ImVec2(mousePosition.x + 14.0f, mousePosition.y + 8.0f),
                                  IM_COL32(255, 255, 255, 230),
                                  isMesh ? "Drop mesh" : "Drop prefab");

                if (payload->IsDelivery())
                {
                    const glm::ivec2 size{
                        std::max(1, static_cast<int>(m_imageRight - m_imageLeft)),
                        std::max(1, static_cast<int>(m_imageBottom - m_imageTop))
                    };
                    const glm::vec2 dropPosition{
                        std::clamp(mousePosition.x - m_imageLeft, 0.0f, static_cast<float>(size.x)),
                        std::clamp(mousePosition.y - m_imageTop, 0.0f, static_cast<float>(size.y))
                    };
                    const glm::vec3 position = getCreationPositionAt(dropPosition);
                    if (isMesh) {
                        CreateMeshEntity(*scene, *currentAsset, position);
                    } else {
                        InstantiatePrefabAt(*scene, *currentAsset, position);
                    }
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (m_openCreatePopup)
    {
        ImGui::SetNextWindowPos(ImVec2(m_contextPopupPosition.x, m_contextPopupPosition.y));
        ImGui::OpenPopup("ViewportCreatePopup");
        m_openCreatePopup = false;
    }

    if (ImGui::BeginPopup("ViewportCreatePopup"))
    {
        const glm::ivec2 size{
            std::max(1, static_cast<int>(m_imageRight - m_imageLeft)),
            std::max(1, static_cast<int>(m_imageBottom - m_imageTop))
        };
        const glm::vec2 clickPosition{
            std::clamp(m_contextClickPosition.x, 0.0f, static_cast<float>(size.x)),
            std::clamp(m_contextClickPosition.y, 0.0f, static_cast<float>(size.y))
        };
        const glm::vec3 creationPosition = getCreationPositionAt(clickPosition);
        g_UIManager->drawEntityCreationMenu(*scene, creationPosition);
        ImGui::EndPopup();
    }

    if (camera.isCapturingMouse())
    {
        ImGui::SetWindowFocus();
    }
    
    if (!scene->isSimulationMode() && !ActiveEditSession(*scene))
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
