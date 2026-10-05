//
//  UIManager.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 08.08.26.
//

#include "UIManager.h"

#include "Application.h"
#include "RenderDevice.h"

#include "ViewportPanel.h"
#include "InspectorPanel.h"
#include "MaterialPanel.h"
#include "AssetsPanel.h"
#include "ScenePanel.h"
#include "HierarchyPanel.h"
#include "Viewport.h"

#include "SceneManager.h"
#include "World.h"

#include "ScriptEngine.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "../vendor/tinyfiledialogs.h"

void DrawScriptConsoleWindow();

UIManager::UIManager()
{
    m_pMainView = new Viewport();
    m_pMainView->init();
    
    m_pViewportPanel = new ViewportPanel("Perspective");
    m_pViewportPanel->setViewport(m_pMainView);
    m_pViewportPanel->m_manager = this;
    
    m_pScenePanel = new ScenePanel();
    m_pScenePanel->m_manager = this;
    
    m_pHierarchyPanel = new HierarchyPanel();
    m_pHierarchyPanel->m_manager = this;
    
    m_pInspectorPanel = new InspectorPanel();
    m_pInspectorPanel->m_manager = this;
    
    m_pAssetsPanel = new AssetsPanel();
    m_pAssetsPanel->m_manager = this;
    
    m_pMaterialPanel = new MaterialPanel();
    m_pMaterialPanel->m_manager = this;
    m_pMaterialPanel->isVisible = false;
    
    m_panels.push_back(m_pViewportPanel);
    m_panels.push_back(m_pScenePanel);
    m_panels.push_back(m_pHierarchyPanel);
    m_panels.push_back(m_pInspectorPanel);
    m_panels.push_back(m_pMaterialPanel);
    m_panels.push_back(m_pAssetsPanel);
    
    g_UIManager = this;
}

UIManager::~UIManager()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
}

void UIManager::update(float dt)
{
    for (auto panel : m_panels)
    {
        panel->update(dt);
    }
    
    if (m_is_picking_mode) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
}

void UIManager::draw()
{
    glEnable(GL_FRAMEBUFFER_SRGB);
    
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    drawMenuBar();
    
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags flags =
    ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
    ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("DockSpace", nullptr, flags);
    ImGui::PopStyleVar();
    ImGui::DockSpace(ImGui::GetID("MainDockSpace"));
    ImGui::End();
    
    drawPanels();
    
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}


//// FIXME: Это жуткий костыль
//void UIManager::setScene(WorldScene *scene)
//{
//    m_pScenePanel->setScene(scene);
//    scene->m_pAttachedViewport = m_pMainView;
//}

void UIManager::init(struct GLFWwindow *window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");
    
    io.FontDefault = io.Fonts->AddFontFromFileTTF("assets/fonts/Ruda/Ruda-Medium.ttf", 17.0f);
    io.Fonts->Fonts[1] = io.Fonts->AddFontFromFileTTF("assets/fonts/Ruda/Ruda-Bold.ttf", 17.0f);
//    io.Fonts->Fonts[2] = io.Fonts->AddFontFromFileTTF("assets/fonts/Ruda/Ruda-Regular.ttf", 14.0f);
    
    // Rounding
    ImGui::GetStyle().WindowRounding = 0.0;
    ImGui::GetStyle().ChildRounding = 4.0;
    ImGui::GetStyle().FrameRounding = 4.0;
    ImGui::GetStyle().TabRounding = 0.0;
    ImGui::GetStyle().PopupRounding = 0.0;
    ImGui::GetStyle().GrabRounding = 3.0;
    
    ImGui::GetStyle().PopupBorderSize = 0.0;
    
    ImGui::GetStyle().SeparatorTextBorderSize = 1.0f;
    
    // Padding
    ImGui::GetStyle().FramePadding = ImVec2(6, 3);
    
    // Size
    ImGui::GetStyle().GrabMinSize = 11.0;
    
    ImGui::GetStyle().TabBarOverlineSize = 0.0f;
    
    ImGui::GetStyle().DragDropTargetRounding = 0;
    ImGui::GetStyle().DragDropTargetBorderSize = 1;
    
    // Show/Hide
    ImGui::GetStyle().WindowMenuButtonPosition = ImGuiDir::ImGuiDir_None;
    
    ImVec4* colors = ImGui::GetStyle().Colors;
    
    colors[ImGuiCol_WindowBg]            = ImVec4(29.0f/255, 29.0f/255, 37.0f/255, 1.0);
    colors[ImGuiCol_PopupBg]             = ImVec4(0.15, 0.15, 0.15, 0.95);
    colors[ImGuiCol_Text]                = ImVec4(250.0f/255, 250.0f/255, 250.0f/255, 0.6);
    
    // Follow Main Theme Color
    colors[ImGuiCol_DockingPreview]      = ImVec4(0.84, 0.65, 0.19, 1.0);
    colors[ImGuiCol_CheckMark]           = ImVec4(0.84, 0.65, 0.19, 1.0);
    colors[ImGuiCol_SliderGrab]          = ImVec4(0.84, 0.65, 0.19, 1.0);
    colors[ImGuiCol_SliderGrabActive]    = ImVec4(0.84, 0.65, 0.19, 1.0);
    colors[ImGuiCol_ResizeGrip]          = ImVec4(0.84, 0.65, 0.19, 1.0);
    
    // Headers
    colors[ImGuiCol_Header]              = ImVec4(0.20, 0.21, 0.21, 1.0);
    colors[ImGuiCol_HeaderHovered]       = ImVec4(0.30, 0.30, 0.30, 1.0);
    colors[ImGuiCol_HeaderActive]        = ImVec4(0.15, 0.15, 0.15, 1.0);
    
    // Buttons
    colors[ImGuiCol_Button]              = ImVec4(0.20, 0.21, 0.21, 1.0);
    colors[ImGuiCol_ButtonHovered]       = ImVec4(0.30, 0.30, 0.30, 1.0);
    colors[ImGuiCol_ButtonActive]        = ImVec4(0.15, 0.15, 0.15, 1.0);
    
    // Frame BG
    colors[ImGuiCol_FrameBg]             = ImVec4(0.20, 0.21, 0.21, 1.0);
    colors[ImGuiCol_FrameBgHovered]      = ImVec4(0.30, 0.30, 0.30, 1.0);
    colors[ImGuiCol_FrameBgActive]       = ImVec4(0.15, 0.15, 0.15, 1.0);
    
    // Tabs
    colors[ImGuiCol_Tab]                 = ImVec4(50.0f/255, 50.0f/255, 58.0f/255, 0.0);
    colors[ImGuiCol_TabHovered]          = ImVec4(29.0f/255, 29.0f/255, 37.0f/255, 1.0);
    colors[ImGuiCol_TabActive]           = ImVec4(29.0f/255, 29.0f/255, 37.0f/255, 1.0);
    colors[ImGuiCol_TabDimmed]           = ImVec4(50.0f/255, 50.0f/255, 58.0f/255, 0.0);
    colors[ImGuiCol_TabDimmedSelected]   = ImVec4(29.0f/255, 29.0f/255, 37.0f/255, 1.0);
    
    // Title
    colors[ImGuiCol_TitleBg]             = ImVec4(50.0f/255, 50.0f/255, 58.0f/255, 1.0);
    colors[ImGuiCol_TitleBgActive]       = ImVec4(50.0f/255, 50.0f/255, 58.0f/255, 1.0);
    colors[ImGuiCol_TitleBgCollapsed]    = ImVec4(50.0f/255, 50.0f/255, 58.0f/255, 1.0);
    
    // Separator
    colors[ImGuiCol_Separator]           = ImVec4(63.0f/255, 68.0f/255, 81.0f/255, 1.0);
    colors[ImGuiCol_SeparatorHovered]    = ImVec4(63.0f/255, 68.0f/255, 81.0f/255, 1.0);
    colors[ImGuiCol_SeparatorActive]     = ImVec4(63.0f/255, 68.0f/255, 81.0f/255, 1.0);
    
    //    // ScrollBar
    colors[ImGuiCol_ScrollbarBg]         = ImVec4(0.15, 0.15, 0.15, 1.0);
}

#include "SceneSerializer.h"
#include "SceneManager.h"

void UIManager::drawMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        auto activeScene = SceneManager::Get().getActiveScene();
        
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Load")) {
                char const * filter[3] = { "*.json", "*.scene", "*.prefab" };
                const char* path = tinyfd_openFileDialog("Open Scene or Prefab", "./assets/", 3, filter, NULL, 0);
                if (path != nullptr) SceneManager::Get().openFile(path);
            }
            
            if (ImGui::MenuItem("Save")) {
                SceneManager::Get().saveCurrent();
            }
            
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {}
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, false)) {} // Disabled item
            
            ImGui::Separator();
            
            if (ImGui::MenuItem("Cut", "Ctrl+X")) {}
            if (ImGui::MenuItem("Copy", "Ctrl+C")) {}
            if (ImGui::MenuItem("Paste", "Ctrl+V")) {}
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View"))
        {
            for (auto panel : m_panels)
            {
                if (ImGui::MenuItem(panel->m_name.c_str(), NULL, panel->isVisible))
                {
                    panel->isVisible = !panel->isVisible;
                }
            }
            
            static bool show_demo = false;
            if (ImGui::MenuItem("Demo", NULL, show_demo))
            {
                show_demo = !show_demo;
            }
            
            if (show_demo)
            {
                ImGui::ShowDemoWindow();
            }
            
            ImGui::EndMenu();
        }
        
        
        char fps_text[32];
        snprintf(fps_text, sizeof(fps_text), "FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::SameLine(ImGui::GetWindowWidth() - 64 - 8);
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", fps_text);
        
        
        ImGui::EndMainMenuBar();
    }
}

void UIManager::drawPanels()
{
    for (auto panel : m_panels)
    {
        if (panel->isVisible)
        {
            panel->draw();
        }
    }
    
    DrawScriptConsoleWindow();
    
    if (ImGui::IsKeyDown(ImGuiKey_Escape))
    {
        if (g_UIManager->m_is_picking_mode)
        {
            g_UIManager->endPicking();
        }
    }
}

void UIManager::setEntity(Entity *entity)
{
    m_pInspectorPanel->setEntity(entity);
    m_pViewportPanel->setEntity(entity);
}

void UIManager::setMaterial(Material* material)
{
    m_pMaterialPanel->setMaterial(material);
}

void UIManager::startPicking(std::function<void (Entity *)> callback)
{
    m_active_picking_callback = callback;
    m_is_picking_mode = true;
}

void UIManager::endPicking()
{
    m_active_picking_callback = nullptr;
    m_is_picking_mode = false;
}

void DrawScriptConsoleWindow()
{
    ImGui::Begin("Script Console");
    
    // Кнопка очистки консоли
    if (ImGui::Button("Clear")) {
        ScriptEngine::get().ClearLogs();
    }
    
    ImGui::Separator();
    
    const auto& logs = ScriptEngine::get().GetLogMessages();
    
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true);
    
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(logs.size()));
    
    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        {
            const auto& log = logs[i];
            
            ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // Белый по умолчанию (Info)
            const char* prefix = "[INFO]";
            
            if (log.type == ScriptLogType::Warning) {
                color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); // Желтый (Warning)
                prefix = "[WARN]";
            }
            else if (log.type == ScriptLogType::Error) {
                color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); // Красный (Error)
                prefix = "[ERRO]";
            }
            else if (log.type == ScriptLogType::ScriptPrint) {
                color = ImVec4(0.4f, 1.0f, 0.4f, 1.0f); // Светло-зеленый для print()
                prefix = "[SCRIPT]";
            }
            
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            
            if (log.type == ScriptLogType::ScriptPrint)
            {
                ImGui::TextWrapped("%s %s", prefix, log.text.c_str());
            }
            else
            {
                ImGui::TextWrapped("%s %s(%d, %d): %s",
                                   prefix,
                                   log.section.c_str(),
                                   log.row,
                                   log.col,
                                   log.text.c_str()
                                   );
            }
            
            ImGui::PopStyleColor();
        }
    }
    ImGui::EndChild();
    
    ImGui::End();
}
