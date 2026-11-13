//
//  Application.cpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 30.11.24.
//

#include "Application.h"

#include "Renderer.h"

#include "RenderDevice.h"
#include "AudioSystem.h"
#include "GizmoRender.h"
#include "renderer/OverlayRender.h"
#include "SpriteRenderer.h"
#include "ParticleSystem.h"
#include "DecalManager.h"
#include "GameUI.h"

#include "SceneManager.h"
#include "UIManager.h"
#include "World.h"
#include "Input.h"
#include "ViewDesc.h"

#include "PhysicsSystem.h"

#include "trenchbroom/LightmapBakeService.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>

#include <stdio.h>
#include <filesystem>
#include <thread>

#if defined(_M_X64) || defined(__x86_64__)
#include <immintrin.h>
#endif

static void error_callback(int e, const char *d) { printf("Error %d: %s\n", e, d); }

static void window_focus_callback(GLFWwindow* window, int focused) {
    if (!focused) {
        ImGuiIO& io = ImGui::GetIO();
        io.ClearInputKeys();
    }
}

static Application* s_currentInstance = nullptr;

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

    const int window_width = 1440;
    const int window_height = 900;
    m_pWindow = glfwCreateWindow(window_width, window_height, "Cucumber", nullptr, nullptr);

    glfwMakeContextCurrent(m_pWindow);
    glfwSetWindowFocusCallback(m_pWindow, window_focus_callback);

    initSubSystems();
    
    m_pUIManager = new UIManager();
    m_pUIManager->init(m_pWindow);
    
    // У нас свое ограничение частоты кадров
    glfwSwapInterval(0);
    
    s_currentInstance = this;
}

Application::~Application()
{
    if (m_pUIManager)
    {
        delete m_pUIManager;
    }
    
    if (m_pRenderer) m_pRenderer->shutdown();
    
    glfwDestroyWindow(m_pWindow);
    glfwTerminate();
}

void Application::run()
{
    double lastTime = glfwGetTime();
    const double desiredFrameTime = 1.0 / 60;
    std::string currentWindowTitle;
    
    while (!glfwWindowShouldClose(m_pWindow))
    {
        double currTime = glfwGetTime();
        double elapsedTime = currTime - lastTime;
        
        // Ограничитель FPS
        if (elapsedTime < desiredFrameTime) {
            double timeToWait = desiredFrameTime - elapsedTime;
            
            // Спим только если ждать нужно дольше 2 мс
            if (timeToWait > 0.003) {
                long long sleepMs = static_cast<long long>((timeToWait - 0.003) * 1000.0);
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
            }
            
            // Точное дожигание остатка времени
            while (glfwGetTime() - lastTime < desiredFrameTime) {
#if defined(_M_X64) || defined(__x86_64__)
                _mm_pause();
#endif
            }
        }
        
        // --- ТОЧКА НАЧАЛА НОВОГО КАДРА ---
        double now = glfwGetTime();
        elapsedTime = now - lastTime; // Истинное время, прошедшее с ПРОШЛОГО кадра
        lastTime = now;      // Фиксируем для следующей итерации
        
        glfwPollEvents();
        updateInputState();
        
        m_pUIManager->startFrame();
        m_pUIManager->update(elapsedTime);
        
        auto activeScene = SceneManager::Get().getActiveScene();
        std::string sceneName = "Untitled";
        if (activeScene && !activeScene->m_filePath.empty()) {
            const std::string fileName = std::filesystem::path(activeScene->m_filePath).stem().string();
            if (!fileName.empty()) sceneName = fileName;
        }
        const std::string windowTitle = "Cucumber - " + sceneName;
        if (windowTitle != currentWindowTitle) {
            glfwSetWindowTitle(m_pWindow, windowTitle.c_str());
            currentWindowTitle = windowTitle;
        }

        if (activeScene)
        {
            activeScene->update(elapsedTime);
        }
        
        ParticleSystem::Update(elapsedTime);
        DecalManager::Update(elapsedTime);
        
        LightmapBakeService::Get().update();
        
        m_pRenderer->renderFrame(activeScene ? &*activeScene : nullptr, *m_pUIManager);
        
        m_pUIManager->draw();

        glfwSwapBuffers(m_pWindow);
    }
}

void Application::updateInputState()
{
    Input::update(m_pWindow);
}

void Application::setCursorEnabled(bool state)
{
    if(s_currentInstance == nullptr) return;

    s_currentInstance->m_cursorEnabled = state;

    if (state)
    {
        ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        glfwSetInputMode(s_currentInstance->m_pWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    else
    {
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
        glfwSetInputMode(s_currentInstance->m_pWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
}

void Application::initSubSystems()
{
    RenderDevice::init();
    
    m_pRenderer = std::make_unique<Renderer>();   // настройки теней: Renderer(ShadowSettings{...})
    m_pRenderer->init();                          // создаёт карты теней, нужен GL
    
    GizmoRender::Get().Initialize();
    OverlayRender::Get().Initialize();
    
    AudioSystem::Get().Initialize();
    SpriteRenderer::Init();
    ParticleSystem::Init();
    DecalManager::Init();
    GameUI::Init();
    
    PhysicsSystem::Get().Init();
}

static void UpdateAudioListener(const std::vector<ViewDesc>& views)
{
    for (const ViewDesc& v : views)
    {
        if (!(v.flags & ViewFlag::AudioListener)) continue;
        
        ma_engine* engine = AudioSystem::Get().GetEngine();
        const glm::vec3& p = v.camera.position;
        const glm::vec3& f = v.camera.forward;
        ma_engine_listener_set_position(engine, 0, p.x, p.y, p.z);
        ma_engine_listener_set_direction(engine, 0, f.x, f.y, f.z);
        ma_engine_listener_set_world_up(engine, 0, 0, 1, 0);
        return;
    }
}
