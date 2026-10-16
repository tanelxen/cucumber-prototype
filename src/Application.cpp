//
//  Application.cpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 30.11.24.
//

#include "Application.h"

#include "RenderDevice.h"
#include "AudioSystem.h"
#include "GizmoRender.h"
#include "SpriteRenderer.h"
#include "ParticleSystem.h"
#include "DecalManager.h"
#include "GameUI.h"

#include "SceneManager.h"
#include "UIManager.h"
#include "World.h"
#include "Input.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>

#include <stdio.h>
#include <thread>

#if defined(_M_X64) || defined(__x86_64__)
#include <immintrin.h>
#endif

static void error_callback(int e, const char *d) { printf("Error %d: %s\n", e, d); }

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
        m_pUIManager->saveProjectSettings();
        delete m_pUIManager;
    }
    glfwDestroyWindow(m_pWindow);
    glfwTerminate();
}

void Application::run()
{
    double lastTime = glfwGetTime();
    const double desiredFrameTime = 1.0 / 60;
    
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
        
        m_pUIManager->update(elapsedTime);
        
        auto activeScene = SceneManager::Get().getActiveScene();
        if (activeScene)
        {
            activeScene->update(elapsedTime);
            activeScene->prepareRenderViews();
        }
        
        ParticleSystem::Update(elapsedTime);
        DecalManager::Update(elapsedTime);
        
        m_pUIManager->preRender();
        RenderDevice::flush();
        
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
    GizmoRender::Get().Initialize();
    
    AudioSystem::Get().Initialize();
    SpriteRenderer::Init();
    ParticleSystem::Init();
    DecalManager::Init();
    GameUI::Init();
}
