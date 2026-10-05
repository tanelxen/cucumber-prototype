//
//  Application.cpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 30.11.24.
//

#include "Application.h"

#include "RenderDevice.h"
#include "AudioSystem.h"
#include "ScriptEngine.h"
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

static void error_callback(int e, const char *d) { printf("Error %d: %s\n", e, d); }

Application* Application::currentInstance = nullptr;

int kek();

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
    
    // Инициализируем стартовую пустую сцену в менеджере!
    SceneManager::Get().createNewScene();
    
    // У нас свое ограничение частоты кадров
    glfwSwapInterval(0);
    
    currentInstance = this;
}

Application::~Application()
{
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
        }
        
        ParticleSystem::Update(elapsedTime);
        DecalManager::Update(elapsedTime);
        
        if (activeScene)
        {
            activeScene->draw();
        }
        
        m_pUIManager->draw();

        glfwSwapBuffers(m_pWindow);
    }
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
    if(currentInstance == nullptr) return;
    
    if (state)
    {
        ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        glfwSetInputMode(currentInstance->m_pWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    else
    {
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
        glfwSetInputMode(currentInstance->m_pWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
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
    
    if (ScriptEngine::get().init())
    {
        ScriptEngine::get().CompileAllScripts("assets/scripts");
    }
    else
    {
        std::cerr << "Failed to init AngelScript!" << std::endl;
    }
}
