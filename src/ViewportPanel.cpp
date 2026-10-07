//
//  ViewportPanel.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#include "ViewportPanel.h"
#include "Viewport.h"

#include "RenderDevice.h"

#include "CameraController.h"
#include "CameraComponent.h"
#include "Camera.h"

#include "SceneManager.h"
#include "World.h"
#include "Entity.h"

#include <glad/glad.h>
#include <imgui.h>

#include <ImGuizmo.h>

static bool DrawPlayButton();
static bool DrawStopButton();

static void DrawGizmo(Entity* selectedNode, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const ImVec2& contentPos, const ImVec2& contentSize);

ViewportPanel::ViewportPanel(const std::string &name) : EditorPanel(name)
{
    m_pCamera = new Camera();
    
    m_pCameraController = new CameraController(m_pCamera);
    m_pCameraController->position = {0, 128, 128};
}

void ViewportPanel::setViewport(Viewport *viewport)
{
    m_viewport = viewport;
    m_viewport->setCamera(m_pCamera);
}

void ViewportPanel::setEntity(Entity *entity)
{
    m_entity = entity;
}

void ViewportPanel::update(float dt)
{
    auto activeScene = SceneManager::Get().getActiveScene();
    if (!activeScene) return;
    
    // Проверяем, должна ли сейчас двигаться камера редактора
    // Если идет игра И на сцене есть живая камера игрока — контроллер редактора отключается
    if (activeScene->isSimulationMode() && activeScene->hasActiveSceneCamera())
    {
        // Камера игрока управляется своими игровыми скриптами (MovementComponent и т.д.)
        return;
    }
    
    // В противном случае (мы в редакторе ИЛИ в игре нет камеры) — камера редактора работает!
    m_pCameraController->update(dt);
}

void ViewportPanel::preRender()
{
    auto activeScene = SceneManager::Get().getActiveScene();
    if (!activeScene) return;
    
    // Умный пропуск: Если сцена в режиме симуляции САМА сгенерировала игровые проходы
    // в наш FBO (m_viewport), то панели редактора генерировать свой проход НЕ НАДО.
    if (activeScene->isSimulationMode() && activeScene->hasActiveSceneCamera())
    {
        return;
    }
    
    // РЕДАКТОРНЫЙ РЕЖИМ (или фоллбэк, если в игре нет камеры):
    // Строим обычный кадр от камеры вьюпорта
    RenderView editorPass = m_viewport->createRenderView();
    activeScene->collectRenderCommands(editorPass);
    RenderDevice::submitView(editorPass);
}

void ViewportPanel::draw()
{
    auto activeScene = SceneManager::Get().getActiveScene();
    if (!activeScene) return;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
    ImGui::Begin(m_name.c_str());
    
    ImGui::Checkbox("Grid", &m_viewport->drawGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Gizmos", &m_viewport->drawGizmos);
    ImGui::SameLine();
    ImGui::Checkbox("Sky", &m_viewport->drawSkybox);
    
    ImGui::SameLine(ImGui::GetWindowWidth() - 48);
    
    if (activeScene->isSimulationMode())
    {
        if (DrawStopButton())
        {
            SceneManager::Get().stopSimulation();
            m_viewport->setCamera(m_pCamera);
        }
    }
    else
    {
        if (DrawPlayButton())
        {
            SceneManager::Get().startSimulation(m_viewport);
        }
    }
    
    float fbo_width = (float)m_viewport->getWidth();
    float fbo_height = (float)m_viewport->getHeight();
    
    ImVec2 current_avail_size = ImGui::GetContentRegionAvail();
    
    if (current_avail_size.x != fbo_width || current_avail_size.y != fbo_height)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            m_isResizing = true;
            m_pendingWidth = current_avail_size.x;
            m_pendingHeight = current_avail_size.y;
        }
        else
        {
            m_viewport->resize((int)current_avail_size.x, (int)current_avail_size.y);
            m_isResizing = false;
        }
    }
    
    ImVec2 contentPos = ImGui::GetCursorScreenPos();
    ImVec2 contentSize = ImGui::GetContentRegionAvail();
    
    ImGui::Image((ImTextureID)(intptr_t)m_viewport->getTexture(),
                 current_avail_size,
                 ImVec2(0, 1),
                 ImVec2(1, 0)
                 );
    
    m_isHovered = ImGui::IsItemHovered();
    
    if (m_pCameraController->isFlyMode())
    {
        ImGui::SetWindowFocus();
    }
    
    if (activeScene->isSimulationMode() == false)
    {
        auto camera = m_viewport->getCamera();
        DrawGizmo(m_entity, camera->view, camera->projection, contentPos, contentSize);
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

static void DrawGizmo(Entity* selectedNode, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const ImVec2& contentPos, const ImVec2& contentSize)
{
    ImGuizmo::AllowAxisFlip(false);
    
    if (!selectedNode) return;
    
    // 1. Инициализация ImGuizmo для текущего кадра
    ImGuizmo::SetOrthographic(false); // true, если у вас ортографическая камера
    ImGuizmo::BeginFrame();
    ImGuizmo::SetDrawlist();
    
    ImVec2 current_avail_size = ImGui::GetContentRegionAvail();
    
    // 2. Задаем область рисования гизмо (должна совпадать с размерами окна вашего вьюпорта)
    ImGuizmo::SetRect(contentPos.x, contentPos.y, contentSize.x, contentSize.y);
    
    // 3. Получаем ТЕКУЩУЮ мировую матрицу объекта
    auto& transform = selectedNode->getTransform();
    glm::mat4 worldMatrix = transform.getWorldMatrix(); // Заберет из кэша или посчитает
    
    // Вспомогательный массив, так как ImGuizmo принимает сырые float*
    const float* viewPtr = &viewMatrix[0][0];
    const float* projPtr = &projectionMatrix[0][0];
    float* matrixPtr = &worldMatrix[0][0];
    
    // 4. Отрисовка и манипуляция гизмо
    // Эта функция сама проверяет ввод мыши и модифицирует matrixPtr (worldMatrix)
    
    float snap[3] = {16.f, 16.f, 16.f};
    ImGuizmo::Manipulate(viewPtr, projPtr, ImGuizmo::TRANSLATE, ImGuizmo::LOCAL, matrixPtr, NULL, snap);
    
    // 5. Если пользователь потянул за стрелочку — применяем изменения
    if (ImGuizmo::IsUsing())
    {
        // Передаем измененную мировую матрицу обратно в трансформ
        transform.setWorldMatrix(worldMatrix);
    }
}
