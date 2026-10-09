//
//  Input.hpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 30.11.24.
//

#pragma once

#define KEY_SPACE 32
#define KEY_W 87
#define KEY_S 83
#define KEY_A 65
#define KEY_D 68

struct GLFWwindow;

struct Input
{
    struct State
    {
        bool keys[256] = {};
        bool previousKeys[256] = {};
        bool leftMouseButtonPressed = false;
        bool previousLeftMouseButtonPressed = false;
        bool rightMouseButtonPressed = false;
        bool previousRightMouseButtonPressed = false;
        float mouseX = 0;
        float mouseY = 0;
        float mouseOffsetX = 0;
        float mouseOffsetY = 0;

        bool isKeyPressed(int keyNum) const;
        bool isKeyJustPressed(int keyNum) const;
        bool isKeyClicked(int keyNum) const;
        bool isLeftMouseButtonPressed() const;
        bool isLeftMouseButtonJustPressed() const;
        bool isRightMouseButtonPressed() const;
        bool isLeftMouseButtonClicked() const;
        bool isRightMouseButtonClicked() const;
        float getMouseOffsetX() const { return mouseOffsetX; }
        float getMouseOffsetY() const { return mouseOffsetY; }
        float getMousePositionX() const { return mouseX; }
        float getMousePositionY() const { return mouseY; }
    };

    static State getState();
    static void setGameInputState(const State& state);
    static void setGameInputRoutingEnabled(bool enabled);

    static bool isKeyPressed(int keyNum);
    static bool isKeyJustPressed(int keyNum);
    static bool isKeyClicked(int keyNum);
    
    static bool isLeftMouseButtonPressed();
    static bool isLeftMouseButtonJustPressed();
    static bool isRightMouseButtonPressed();
    
    static bool isLeftMouseButtonClicked();
    static bool isRightMouseButtonClicked();
    
    static float getMouseOffsetX();
    static float getMouseOffsetY();
    
    static float getMousePositionX();
    static float getMousePositionY();

    static void setCursorEnabled(bool state);
    static bool isCursorEnabled();

private:
    static Input& getInstance()
    {
        static Input instance;
        return instance;
    }
    
    static const State& getCurrentState();
    static void update(GLFWwindow* defaultWindow);

    friend struct Application;
    
    State m_physicalState;
    State m_gameState;
    bool m_gameInputRoutingEnabled = false;
    bool m_cursorEnabled = true;
};
