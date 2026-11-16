//
//  Application.hpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 30.11.24.
//

#pragma once

#include <memory>

class Renderer;

struct Application
{
    Application();
    ~Application();
    
    void run();
    
    static void setCursorEnabled(bool state);
    
private:
    struct GLFWwindow* m_pWindow = nullptr;
    class UIManager* m_pUIManager = nullptr;
    std::unique_ptr<Renderer> m_pRenderer;
    bool m_cursorEnabled = true;
    
    void updateInputState();
    
private:
    void initSubSystems();
};
