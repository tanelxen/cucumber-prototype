//
//  Application.hpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 30.11.24.
//

#pragma once

struct Application
{
    Application();
    ~Application();
    
    void run();
    
    static void setCursorEnabled(bool state);
    
//private:
    struct GLFWwindow* m_pWindow;
    class WorldScene* m_pScene;
    
    class UIManager* m_pUIManager;
    
    void updateInputState();
    
    static Application* currentInstance;
};
