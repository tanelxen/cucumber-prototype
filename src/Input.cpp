//
//  Input.cpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 30.11.24.
//

#include "Input.h"
#include "Application.h"

#include <GLFW/glfw3.h>

bool Input::State::isKeyPressed(int keyNum) const
{
    if (keyNum < 0 || keyNum >= 256) return false;
    return keys[keyNum];
}

bool Input::State::isKeyJustPressed(int keyNum) const
{
    if (keyNum < 0 || keyNum >= 256) return false;
    return keys[keyNum] && !previousKeys[keyNum];
}

bool Input::State::isKeyClicked(int keyNum) const
{
    if (keyNum < 0 || keyNum >= 256) return false;
    return !keys[keyNum] && previousKeys[keyNum];
}

bool Input::State::isLeftMouseButtonPressed() const
{
    return leftMouseButtonPressed;
}

bool Input::State::isLeftMouseButtonJustPressed() const
{
    return leftMouseButtonPressed && !previousLeftMouseButtonPressed;
}

bool Input::State::isRightMouseButtonPressed() const
{
    return rightMouseButtonPressed;
}

bool Input::State::isLeftMouseButtonClicked() const
{
    return !leftMouseButtonPressed && previousLeftMouseButtonPressed;
}

bool Input::State::isRightMouseButtonClicked() const
{
    return !rightMouseButtonPressed && previousRightMouseButtonPressed;
}

Input::State Input::getState()
{
    return getInstance().m_physicalState;
}

void Input::setGameInputState(const State& state)
{
    getInstance().m_gameState = state;
}

void Input::setGameInputRoutingEnabled(bool enabled)
{
    Input& input = getInstance();
    if (enabled != input.m_gameInputRoutingEnabled)
    {
        input.m_gameState = {};
    }
    input.m_gameInputRoutingEnabled = enabled;
}

const Input::State& Input::getCurrentState()
{
    const Input& input = getInstance();
    return input.m_gameInputRoutingEnabled ? input.m_gameState : input.m_physicalState;
}

void Input::update(GLFWwindow* defaultWindow)
{
    Input& input = getInstance();
    State nextState;

    if (defaultWindow && glfwGetWindowAttrib(defaultWindow, GLFW_FOCUSED) == GLFW_TRUE)
    {
        double mouseX, mouseY;
        glfwGetCursorPos(defaultWindow, &mouseX, &mouseY);

        nextState.mouseX = static_cast<float>(mouseX);
        nextState.mouseY = static_cast<float>(mouseY);
        nextState.mouseOffsetX = nextState.mouseX - input.m_physicalState.mouseX;
        nextState.mouseOffsetY = nextState.mouseY - input.m_physicalState.mouseY;

        for (int i = 32; i < 256; ++i)
        {
            nextState.keys[i] = glfwGetKey(defaultWindow, i) > GLFW_RELEASE;
            nextState.previousKeys[i] = input.m_physicalState.keys[i];
        }

        nextState.leftMouseButtonPressed =
            glfwGetMouseButton(defaultWindow, GLFW_MOUSE_BUTTON_LEFT) > GLFW_RELEASE;
        nextState.previousLeftMouseButtonPressed = input.m_physicalState.leftMouseButtonPressed;
        nextState.rightMouseButtonPressed =
            glfwGetMouseButton(defaultWindow, GLFW_MOUSE_BUTTON_RIGHT) > GLFW_RELEASE;
        nextState.previousRightMouseButtonPressed = input.m_physicalState.rightMouseButtonPressed;
    }

    input.m_physicalState = nextState;
}

bool Input::isKeyPressed(int keyNum)
{
    return getCurrentState().isKeyPressed(keyNum);
}

bool Input::isKeyJustPressed(int keyNum)
{
    return getCurrentState().isKeyJustPressed(keyNum);
}

bool Input::isKeyClicked(int keyNum)
{
    return getCurrentState().isKeyClicked(keyNum);
}

bool Input::isLeftMouseButtonPressed()
{
    return getCurrentState().isLeftMouseButtonPressed();
}

bool Input::isLeftMouseButtonJustPressed()
{
    return getCurrentState().isLeftMouseButtonJustPressed();
}

bool Input::isRightMouseButtonPressed()
{
    return getCurrentState().isRightMouseButtonPressed();
}

bool Input::isLeftMouseButtonClicked()
{
    return getCurrentState().isLeftMouseButtonClicked();
}

bool Input::isRightMouseButtonClicked()
{
    return getCurrentState().isRightMouseButtonClicked();
}

float Input::getMouseOffsetX()
{
    return getCurrentState().getMouseOffsetX();
}

float Input::getMouseOffsetY()
{
    return getCurrentState().getMouseOffsetY();
}

float Input::getMousePositionX()
{
    return getCurrentState().getMousePositionX();
}

float Input::getMousePositionY()
{
    return getCurrentState().getMousePositionY();
}

void Input::setCursorEnabled(bool state)
{
    getInstance().m_cursorEnabled = state;
    Application::setCursorEnabled(state);
}

bool Input::isCursorEnabled()
{
    return getInstance().m_cursorEnabled;
}
