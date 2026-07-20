//
//  Application.cpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 30.11.24.
//

#include <stdio.h>
#include <thread>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "World.h"

#include "Camera.h"

#include "Input.h"

#include "Application.h"

#include "ViewportPanel.h"
#include "ScenePanel.h"
#include "InspectorPanel.h"
#include "AssetsPanel.h"

static void error_callback(int e, const char *d) { printf("Error %d: %s\n", e, d); }

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

Application* Application::currentInstance = nullptr;

void imgui_init(GLFWwindow* window);
void imgui_renderMenuBar();

Application::Application()
{
    /* GLFW */
    glfwSetErrorCallback(error_callback);

    if (!glfwInit()) {
        fprintf(stdout, "[GFLW] failed to init!\n");
        exit(1);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GL_TRUE);
#endif

    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
//    glfwWindowHint(GLFW_SAMPLES, 8);

    const int window_width = 1440;
    const int window_height = 900;
    m_pWindow = glfwCreateWindow(window_width, window_height, "Demo", nullptr, nullptr);

    glfwSetKeyCallback(m_pWindow, key_callback);

    glfwMakeContextCurrent(m_pWindow);

    gladLoadGL();

    const unsigned char* version = glGetString(GL_VERSION);
    printf("version: %s\n", version);
    
    const unsigned char* device = glGetString(GL_RENDERER);
    printf("device: %s\n", device);
    
    imgui_init(m_pWindow);
    
    m_pCamera = new Camera();
    m_pScene = new WorldScene(m_pCamera);
    
    glfwSwapInterval(0);
    glFrontFace(GL_CCW);
    
    currentInstance = this;
}

Application::~Application()
{
    delete m_pScene;
    delete m_pCamera;
    
    glfwDestroyWindow(m_pWindow);
    glfwTerminate();
}

void Application::run()
{
    double lastTime = 0;
    const double desiredFrameTime = 1.0 / 60;
    
//    glfwSetInputMode(m_pWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    while (!glfwWindowShouldClose(m_pWindow))
    {
        double currTime = glfwGetTime();
        double elapsedTime = currTime - lastTime;
        
        if (elapsedTime < desiredFrameTime)
        {
            long long diff = (desiredFrameTime - elapsedTime) * 1000;
            std::this_thread::sleep_for(std::chrono::milliseconds(diff));
            continue;
        }
        
        lastTime = currTime;

        int width, height;
        glfwGetFramebufferSize(m_pWindow, &width, &height);

        m_pScene->update(elapsedTime);
        m_pScene->draw();
        
        glViewport(0, 0, width, height);
        
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_FRAMEBUFFER_SRGB);
        
        glClearColor(0.0, 0.0, 0.0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // ####################### IMGUI #######################
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        
        ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_MenuBar;
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("DockSpace", nullptr, flags);
        ImGui::PopStyleVar();
        ImGui::DockSpace(ImGui::GetID("MainDockSpace"));
        
        imgui_renderMenuBar();
        
        ImGui::End();
        
        m_pScene->m_pViewportPanel->draw();
        m_pScene->m_pScenePanel->draw();
        m_pScene->m_pInspectorPanel->draw();
        m_pScene->m_pAssetsPanel->draw();
        
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        // ####################### IMGUI #######################

        glfwSwapBuffers(m_pWindow);
        
        glfwPollEvents();
        updateInputState();
    }
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
}


void Application::updateInputState()
{
    static bool isFirstFrame = true;
    
    for (int i = 32; i < 256; ++i)
    {
        Input::getInstance().prev_keys[i] = Input::getInstance().keys[i];
        Input::getInstance().keys[i] = glfwGetKey(m_pWindow, i) > GLFW_RELEASE;
    }
    
    Input::getInstance().m_prevLeftMouseButtonState = Input::getInstance().m_currLeftMouseButtonState;
    Input::getInstance().m_prevRightMouseButtonState = Input::getInstance().m_currRightMouseButtonState;
    
    Input::getInstance().m_currLeftMouseButtonState = glfwGetMouseButton(m_pWindow, GLFW_MOUSE_BUTTON_LEFT) > GLFW_RELEASE;
    Input::getInstance().m_currRightMouseButtonState = glfwGetMouseButton(m_pWindow, GLFW_MOUSE_BUTTON_RIGHT) > GLFW_RELEASE;
    
    
    
    double mouseX, mouseY;
    glfwGetCursorPos(m_pWindow, &mouseX, &mouseY);
    
    if (isFirstFrame)
    {
        Input::getInstance().m_mouseOffsetX = 0;
        Input::getInstance().m_mouseOffsetY = 0;
    }
    else
    {
        Input::getInstance().m_mouseOffsetX = mouseX - Input::getInstance().m_mouseX;
        Input::getInstance().m_mouseOffsetY = mouseY - Input::getInstance().m_mouseY;
    }
    
    Input::getInstance().m_mouseX = mouseX;
    Input::getInstance().m_mouseY = mouseY;
    
    isFirstFrame = false;
}

void Application::setCursorEnabled(bool state)
{
    if(currentInstance)
    {
        glfwSetInputMode(currentInstance->m_pWindow, GLFW_CURSOR, state ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    }
}

void imgui_init(GLFWwindow* window)
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");
    
    io.FontDefault = io.Fonts->AddFontFromFileTTF("assets/fonts/Ruda/Ruda-Medium.ttf", 17.0f, NULL, NULL);
    
    // Rounding
    ImGui::GetStyle().WindowRounding = 4.0;
    ImGui::GetStyle().ChildRounding = 4.0;
    ImGui::GetStyle().FrameRounding = 4.0;
    ImGui::GetStyle().TabRounding = 0.0;
    ImGui::GetStyle().PopupRounding = 4.0;
    ImGui::GetStyle().GrabRounding = 3.0;
    
    // Padding
    ImGui::GetStyle().FramePadding = ImVec2(6, 3);
    
    // Size
    ImGui::GetStyle().GrabMinSize = 11.0;
    
    // Show/Hide
    ImGui::GetStyle().WindowMenuButtonPosition = ImGuiDir::ImGuiDir_None;
    
    ImVec4* colors = ImGui::GetStyle().Colors;
    
    colors[ImGuiCol_WindowBg]            = ImVec4(0.15, 0.15, 0.15, 1.0);
    colors[ImGuiCol_PopupBg]             = ImVec4(0.15, 0.15, 0.15, 0.95);
    colors[ImGuiCol_Text]                = ImVec4(0.90, 0.90, 0.90, 1.0);
    
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
    colors[ImGuiCol_Tab]                 = ImVec4(0.15, 0.15, 0.15, 1.0);
    colors[ImGuiCol_TabHovered]          = ImVec4(0.38, 0.38, 0.38, 1.0);
    colors[ImGuiCol_TabActive]           = ImVec4(0.28, 0.28, 0.28, 1.0);
    colors[ImGuiCol_TabUnfocused]        = ImVec4(0.15, 0.15, 0.15, 1.0);
    colors[ImGuiCol_TabUnfocusedActive]  = ImVec4(0.20, 0.21, 0.21, 1.0);
    
    // Title
    colors[ImGuiCol_TitleBg]             = ImVec4(0.15, 0.15, 0.15, 1.0);
    colors[ImGuiCol_TitleBgActive]       = ImVec4(0.15, 0.15, 0.15, 1.0);
    colors[ImGuiCol_TitleBgCollapsed]    = ImVec4(0.15, 0.15, 0.15, 1.0);
    
    // Separator
    colors[ImGuiCol_Separator]           = ImVec4(0.23, 0.23, 0.23, 0.5);
    colors[ImGuiCol_SeparatorHovered]    = ImVec4(0.23, 0.23, 0.23, 0.8);
    colors[ImGuiCol_SeparatorActive]     = ImVec4(0.23, 0.23, 0.23, 0.8);

//    // ScrollBar
    colors[ImGuiCol_ScrollbarBg]         = ImVec4(0.15, 0.15, 0.15, 1.0);
}

void imgui_renderMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {};
            
            if (ImGui::MenuItem("Load Scene", "Ctrl+L")) {};
            ImGui::Separator();
            
            if (ImGui::MenuItem("Reload Scripts", "Ctrl+R")) {};
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
        
//        if (ImGui::BeginMenu("GameObject"))
//        {
//            if (ImGui::MenuItem("Cube")) {
//                _createCubeCallback();
//            }
//            ImGui::EndMenu();
//        }
//        
//        if (ImGui::BeginMenu("Run")) {
//            if (ImGui::MenuItem("Compile + Play")) {
//                auto path = sceneM().activeScene()->getPath();
//                sceneM().saveActiveScene(path);
//                editor().projectBuilder()->build();
//                launchRuntime(sceneM().activeScene()->getPath());
//            }
//            
//            if (ImGui::MenuItem("Compile")) {
//                editor().projectBuilder()->build();
//            }
//            
//            if (ImGui::MenuItem("Play")) {
//                launchRuntime(sceneM().activeScene()->getPath());
//            }
//            
//            ImGui::EndMenu();
//        }
        
        ImGui::EndMainMenuBar();
    }
    }
