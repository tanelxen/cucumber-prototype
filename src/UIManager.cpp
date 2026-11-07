//
//  UIManager.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 08.08.26.
//

#include "UIManager.h"

#include "RenderDevice.h"

#include "ViewportPanel.h"
#include "EditorCamera.h"
#include "InspectorPanel.h"
#include "AssetsPanel.h"
#include "HierarchyPanel.h"
#include "LightComponent.h"

#include "SceneManager.h"
#include "World.h"
#include "Entity.h"
#include "ScriptEngine.h"
#include "ProjectManager.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>

#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "../vendor/tinyfiledialogs.h"
#include "../vendor/IconsMaterialDesignIcons.h"

#include "../vendor/stb_image.h"

#include <glad/glad.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>

static void DrawScriptConsoleWindow();

static ImTextureID LoadTextureFromFile(const char* filename);
static ImTextureID s_projectIcon = 0;

UIManager::UIManager()
{
    const char* home = std::getenv("HOME");
    const std::string defaultLocation = home
    ? (std::filesystem::path(home) / "Cucumber Projects").string()
    : std::filesystem::current_path().string();
    std::snprintf(m_projectLocationBuffer, sizeof(m_projectLocationBuffer), "%s", defaultLocation.c_str());
    
    m_pViewportPanel = new ViewportPanel(ICON_MDI_CAMERA " Viewport", 0);
    m_viewportPanels.push_back(m_pViewportPanel);
    m_pViewportPanel->m_manager = this;
    
//    auto m_pViewportPanel2 = new ViewportPanel(ICON_MDI_CAMERA " Viewport2", 1);
//    m_viewportPanels.push_back(m_pViewportPanel2);
//    m_pViewportPanel2->m_manager = this;
    
    m_pHierarchyPanel = new HierarchyPanel();
    m_pHierarchyPanel->m_manager = this;
    
    m_pInspectorPanel = new InspectorPanel();
    m_pInspectorPanel->m_manager = this;
    
    m_pAssetsPanel = new AssetsPanel();
    m_pAssetsPanel->m_manager = this;
    
    m_panels.push_back(m_pViewportPanel);
//    m_panels.push_back(m_pViewportPanel2);
    m_panels.push_back(m_pHierarchyPanel);
    m_panels.push_back(m_pInspectorPanel);
    m_panels.push_back(m_pAssetsPanel);
    
    g_UIManager = this;
    
    s_projectIcon = LoadTextureFromFile("resources/icons/project.png");
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
}

void UIManager::collectViews(WorldScene& scene, std::vector<ViewDesc>& out)
{
    for (auto* panel : m_viewportPanels)
    {
        panel->collectViews(scene, out);
    }
}

void UIManager::draw()
{
    glEnable(GL_FRAMEBUFFER_SRGB);
    
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    if (m_showProjectLauncher)
    {
        drawProjectLauncher();
    }
    else
    {
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
    }
    
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

static void InitFonts(ImGuiIO& io)
{
    // 1. Настраиваем общую конфигурацию для FontAwesome
    ImFontConfig config;
    config.MergeMode = true;        // КРИТИЧЕСКИ ВАЖНО: сливает FA с предыдущим добавленным шрифтом
    config.PixelSnapH = true;       // Четкое выравнивание пикселей для иконок
    config.GlyphMinAdvanceX = 17.0f; // Выравниваем шаг под размер вашего шрифта (17.0f)
    
    static const ImWchar icon_ranges[] = { ICON_MIN_MDI, ICON_MAX_MDI, 0 };
    
    // ----------------------------------------------------
    // СЕКЦИЯ 1: Загрузка Medium шрифта + FontAwesome к нему
    // ----------------------------------------------------
    // Загружаем основной Medium (он автоматически станет Fonts[0])
    io.FontDefault = io.Fonts->AddFontFromFileTTF("resources/fonts/Ruda/Ruda-Medium.ttf", 17.0f);
    
    io.Fonts->AddFontFromFileTTF("resources/fonts/materialdesignicons-webfont.ttf", 14.0f, &config, icon_ranges);
    
    
    // ----------------------------------------------------
    // СЕКЦИЯ 2: Загрузка Bold шрифта + FontAwesome к нему
    // ----------------------------------------------------
    // Загружаем основной Bold (он станет Fonts[1])
    io.Fonts->Fonts[1] = io.Fonts->AddFontFromFileTTF("resources/fonts/Ruda/Ruda-Bold.ttf", 17.0f);
    
    // Сливаем FontAwesome с Bold
    io.Fonts->AddFontFromFileTTF("resources/fonts/materialdesignicons-webfont.ttf", 17.0f, &config, icon_ranges);
    
    
    
    io.Fonts->Fonts[2] = io.Fonts->AddFontFromFileTTF("resources/fonts/Ruda/Ruda-Regular.ttf", 15.0f);
    //    io.Fonts->AddFontFromFileTTF("resources/fonts/materialdesignicons-webfont.ttf", 18.0f, &config, icon_ranges);
}

void UIManager::init(struct GLFWwindow *window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");
    
    InitFonts(io);
    
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
    colors[ImGuiCol_ModalWindowDimBg]    = ImVec4(0.0f, 0.0f, 0.0f, 0.6f);

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
            if (ImGui::MenuItem("Load Scene or Prefab")) {
                char const * filter[3] = { "*.json", "*.scene", "*.prefab" };
                const auto scenesPath = ProjectManager::Get().scenesPath().string();
                const char* path = tinyfd_openFileDialog("Open Scene or Prefab", scenesPath.c_str(), 3, filter, NULL, 0);
                if (path != nullptr) SceneManager::Get().openFile(path);
            }
            
            if (ImGui::MenuItem("Save Current")) {
                SceneManager::Get().saveCurrent();
            }
            
            if (ImGui::MenuItem("Save As..."))
            {
                char const * filter[3] = { "*.json", "*.scene", "*.prefab" };
                const auto scenesPath = ProjectManager::Get().scenesPath().string();
                const char* path = tinyfd_saveFileDialog("Save Prefab", scenesPath.c_str(), 3, filter, NULL);
                
                if (path != nullptr)
                {
                    SceneManager::Get().saveCurrentAs(path);
                }
            }
            
            ImGui::Separator();
            
            if (ImGui::MenuItem("Reload Scripts"))
            {
                ScriptEngine::get().HotReloadAllScripts(ProjectManager::Get().scriptsPath().string());
                
            }
            
            if (ImGui::MenuItem("New Scene"))
            {
                SceneManager::Get().createNewScene();
            }
            
            ImGui::Separator();
            
            if (ImGui::MenuItem("Open Project..."))
            {
                const char* path = tinyfd_selectFolderDialog("Open Project", ".");
                if (path) openProjectPath(path);
            }
            
            ImGui::EndMenu();
        }
        
        /*
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
         */
        
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

void UIManager::drawProjectLauncher()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    // Немного увеличим высоту окна по умолчанию, чтобы комфортно вмещать и форму, и список
    const ImVec2 size(650.0f, 600.0f);
    
    ImGui::SetNextWindowPos(
                            ImVec2(viewport->Pos.x + (viewport->Size.x - size.x) * 0.5f,
                                   viewport->Pos.y + (viewport->Size.y - size.y) * 0.5f),
                            ImGuiCond_Always);
    
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    
    if (!ImGui::Begin("Cucumber Project Manager", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }
    
    // --- ВЕРХНЯЯ ПАНЕЛЬ КНОПОК ---
    if (ImGui::Button("Browse..."))
    {
        const char* path = tinyfd_selectFolderDialog("Open Project", ".");
        if (path) openProjectPath(path);
    }
    
    ImGui::SameLine();
    
    static bool m_showCreateForm = false;
    
    // Кнопка переключает видимость формы создания проекта
    if (ImGui::Button("Create New..."))
    {
        m_showCreateForm = !m_showCreateForm;
        m_projectError.clear();
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // --- ДИНАМИЧЕСКАЯ ФОРМА СОЗДАНИЯ ПРОЕКТА ---
    if (m_showCreateForm)
    {
        ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "Configure New Project");
        ImGui::Spacing();
        
        ImGui::InputText("Project name", m_projectNameBuffer, sizeof(m_projectNameBuffer));
        ImGui::InputText("Location", m_projectLocationBuffer, sizeof(m_projectLocationBuffer));
        ImGui::SameLine();
        
        if (ImGui::Button("Browse...###InlineBrowse"))
        {
            const char* path = tinyfd_selectFolderDialog("Choose project location", m_projectLocationBuffer);
            if (path)
            {
                std::snprintf(m_projectLocationBuffer, sizeof(m_projectLocationBuffer), "%s", path);
            }
        }
        
        ImGui::Spacing();
        
        if (ImGui::Button("Create & Edit", ImVec2(120, 0)))
        {
            std::string error;
            if (ProjectManager::Get().createProject(m_projectLocationBuffer, m_projectNameBuffer, error))
            {
                m_showCreateForm = false; // Закрываем форму при успехе
                openProjectPath(ProjectManager::Get().projectPath());
            }
            else
            {
                m_projectError = std::move(error);
            }
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(80, 0)))
        {
            m_showCreateForm = false; // Просто скрываем форму
            m_projectError.clear();
        }
        
        if (!m_projectError.empty())
        {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Error: %s", m_projectError.c_str());
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }
    
    // --- ДВУХСТРОЧНЫЙ СПИСОК ПРОЕКТОВ ---
    const auto recentProjects = ProjectManager::Get().recentProjects();
    
    if (recentProjects.empty())
    {
        ImGui::TextDisabled("No recent projects");
    }
    else
    {
        ImGui::BeginChild("RecentProjectsList", ImVec2(0, 0), true);
        
        for (const auto& projectPath : recentProjects)
        {
            std::string projectName = projectPath.filename().string();
            std::string fullPath = projectPath.string();
            std::string id = "###" + fullPath;
            
            const float itemHeight = 64.0f;
            ImVec2 selectableSize(0, itemHeight);
            
            ImGui::PushID(fullPath.c_str());
            
            // Запоминаем стартовую позицию элемента
            ImVec2 screenCursorPos = ImGui::GetCursorScreenPos();
            float initialLocalY = ImGui::GetCursorPosY();
            
            // Кликабельная область на всю высоту элемента
            if (ImGui::Selectable(id.c_str(), false, 0, selectableSize))
            {
                openProjectPath(projectPath);
            }
            
            // Переключаемся в режим "рисования поверх"
            // Используем относительный SetCursorPosY вместо абсолютного экрана, чтобы не ломать контейнер
            ImGui::SetCursorPosY(initialLocalY);
            
            // 1. ИКОНКА ПРОЕКТА
            ImGui::SetCursorScreenPos(screenCursorPos + ImVec2(4, 4));
            ImGui::Image(s_projectIcon, ImVec2(56.0f, 56.0f));
            
            // 2. НАЗВАНИЕ ПРОЕКТА
            ImGui::SetCursorScreenPos(screenCursorPos + ImVec2(76.0f, 12.0f));
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
            ImGui::Text("%s", projectName.c_str());
            ImGui::PopFont();
            
            // 3. ПУТЬ К ПРОЕКТУ
            ImGui::SetCursorScreenPos(screenCursorPos + ImVec2(76.0f, 36.0f));
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
            ImGui::TextDisabled("%s", fullPath.c_str());
            ImGui::PopFont();
            
            ImGui::PopID();
            
            // Возвращаем курсор в нормальный поток ImGui строго в конец текущего элемента
            ImGui::SetCursorPosY(initialLocalY + itemHeight);
            
            // Добавляем небольшой отступ между карточками проектов
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
        }
        
        ImGui::EndChild();
    }
    
    ImGui::End();
}


bool UIManager::openProjectPath(const std::filesystem::path& path)
{
    std::string error;
    auto& projectManager = ProjectManager::Get();
    
    if (!projectManager.openProject(path, error))
    {
        m_projectError = std::move(error);
        return false;
    }
    
    SceneManager::Get().createNewScene();
    
    if (!ScriptEngine::get().init(projectManager.scriptsPath().string()))
    {
        m_projectError = "Не удалось инициализировать AngelScript для проекта.";
        return false;
    }
    
    if (!ScriptEngine::get().CompileAllScripts(projectManager.scriptsPath().string()))
    {
        m_projectError = "Не удалось скомпилировать скрипты проекта.";
        return false;
    }
    
    bool sceneOpened = true;
    
    if (projectManager.startScene().empty())
    {
        SceneManager::Get().createNewScene();
    }
    else
    {
        const auto scenePath = projectManager.resolveProjectPath(projectManager.startScene());
        sceneOpened = SceneManager::Get().openFile(scenePath.string());
    }
    
    if (!sceneOpened)
    {
        m_projectError = "Не удалось загрузить стартовую сцену проекта.";
        return false;
    }
    
    m_pAssetsPanel->setRootPath(projectManager.assetsPath());
    
    m_projectError.clear();
    m_showProjectLauncher = false;
    
    return true;
}

EditorCamera& UIManager::editorCamera()
{
    return m_pViewportPanel->viewport().camera();
}

void UIManager::resetEditorCamera()
{
    editorCamera() = EditorCamera{};
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
}

void UIManager::setEntity(Entity *entity)
{
    m_pInspectorPanel->setEntity(entity);
    m_pHierarchyPanel->setEntity(entity);
    
    for (auto* panel : m_viewportPanels)
    {
        panel->setEntity(entity);
    }
}

void UIManager::drawEntityCreationMenu(WorldScene& scene, const glm::vec3& position)
{
    const auto createEntity = [&](const std::string& name, bool addLight)
    {
        auto entity = std::make_unique<Entity>(name);
        entity->scene = &scene;
        entity->getTransform().setPosition(position);

        if (addLight) {
            entity->addComponent<LightComponent>();
        }

        Entity* createdEntity = entity.get();
        scene.m_entities.push_back(std::move(entity));
        setEntity(createdEntity);
    };

    const auto count = std::to_string(scene.m_entities.size());
    if (ImGui::MenuItem("Empty")) {
        createEntity("Entity_" + count, false);
    }
    if (ImGui::MenuItem("Light")) {
        createEntity("Light_" + count, true);
    }
}

static void DrawScriptConsoleWindow()
{
    ImGui::Begin(ICON_MDI_CONSOLE " Script Console###Script Console");
    
    if (ImGui::Button(ICON_MDI_TRASH_CAN " Clear")) {
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

static ImTextureID LoadTextureFromFile(const char* filename)
{
    int width = 0, height = 0, channels = 0;
    // Декодируем PNG/JPG в сырой массив пикселей (RGBA)
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 4);
    if (data == nullptr) return 0;
    
    // Создаем текстуру в OpenGL
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    // Настраиваем фильтрацию (чтобы иконка не размывалась при изменении размеров)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Загружаем пиксели в видеопамять
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB_ALPHA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    
    // Освобождаем память процессора, так как картинка уже на видеокарте
    stbi_image_free(data);
    
    // Приводим GLuint к типу ImTextureID, который ждет ImGui
    return (ImTextureID)(intptr_t)textureID;
}
