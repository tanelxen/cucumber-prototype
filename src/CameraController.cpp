//
//  Player.cpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 03.11.24.
//

#include <GLFW/glfw3.h>

#include "CameraController.h"

#include "Input.h"
#include "Application.h"
#include "Camera.h"

static const float kMouseSense = 0.15;

CameraController::CameraController(Camera *camera) : m_pCamera(camera)
{
    
}

void CameraController::update(float dt)
{
    if (Input::isRightMouseButtonPressed())
    {
        Application::setCursorEnabled(false);
        
        yaw += Input::getMouseOffsetX() * kMouseSense * dt;
        pitch -= Input::getMouseOffsetY() * kMouseSense * dt;
        
        m_isFlyMode = true;
    }
    else
    {
        Application::setCursorEnabled(true);
        m_isFlyMode = false;
    }
    
    if (pitch > 1.5) pitch = 1.5;
    else if (pitch < -1.5) pitch = -1.5;
    
    
    forward.x = cos(yaw) * cos(pitch);
    forward.y = sin(pitch);
    forward.z = sin(yaw) * cos(pitch);
    forward = glm::normalize(forward);
    
    glm::vec3 unit_up = {0, 1, 0};
    
    right = glm::cross(forward, unit_up);
    right = glm::normalize(right);
    
    up = glm::cross(right, forward);
    up = glm::normalize(up);
    
    
    float forwardmove = 0;
    float rightmove = 0;
    float upmove = 0;
    
    if (Input::isKeyPressed(KEY_W))
    {
        forwardmove += 1;
    }
    
    if (Input::isKeyPressed(KEY_S))
    {
        forwardmove -= 1;
    }
    
    if (Input::isKeyPressed(KEY_D))
    {
        rightmove += 1;
    }
    
    if (Input::isKeyPressed(KEY_A))
    {
        rightmove -= 1;
    }
    
    if (Input::isKeyPressed(KEY_SPACE))
    {
        upmove += 1;
    }
    
    if (Input::isKeyPressed(GLFW_KEY_C))
    {
        upmove -= 1;
    }
    
//    bool jump = Input::isKeyPressed(KEY_SPACE);
    
    static float cl_forwardspeed = 400.0;
    static float cl_sidespeed = 350.0;
    
    glm::vec3 direction = {0, 0, 0};
    direction += forward * forwardmove * cl_forwardspeed;
    direction += right * rightmove * cl_forwardspeed;
    direction += up * upmove * cl_forwardspeed;
    
    position += direction * dt;
    
    if (m_pCamera)
    {
        m_pCamera->setTransform(position, forward, right, up);
    }
}
