//
//  Player.cpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 03.11.24.
//

#include <GLFW/glfw3.h>

#include "CameraController.h"

#include "Input.h"
#include "Camera.h"

#include "AudioSystem.h"

static const float kMouseSense = 0.15;

CameraController::CameraController(Camera *camera) : m_pCamera(camera)
{
    
}

void CameraController::update(float dt, const Input::State& input)
{
    if (input.isRightMouseButtonPressed())
    {
        Input::setCursorEnabled(false);
        
        yaw -= input.getMouseOffsetX() * kMouseSense * dt;
        pitch -= input.getMouseOffsetY() * kMouseSense * dt;
        
        m_isFlyMode = true;
    }
    else
    {
        Input::setCursorEnabled(true);
        m_isFlyMode = false;
    }
    
    if (pitch > 1.5) pitch = 1.5;
    else if (pitch < -1.5) pitch = -1.5;
    
    
    forward.x = -sin(yaw) * cos(pitch);
    forward.y =  sin(pitch);
    forward.z = -cos(yaw) * cos(pitch);
    forward = glm::normalize(forward);
    
    right.x =  cos(yaw);
    right.y =  0.0f;
    right.z = -sin(yaw);
    right = glm::normalize(right);
    
    up = glm::cross(right, forward);
    up = glm::normalize(up);
    
    
    float forwardmove = 0;
    float rightmove = 0;
    float upmove = 0;
    
    if (m_isFlyMode)
    {
        if (input.isKeyPressed(KEY_W))
        {
            forwardmove += 1;
        }
        
        if (input.isKeyPressed(KEY_S))
        {
            forwardmove -= 1;
        }
        
        if (input.isKeyPressed(KEY_D))
        {
            rightmove += 1;
        }
        
        if (input.isKeyPressed(KEY_A))
        {
            rightmove -= 1;
        }
        
        if (input.isKeyPressed(KEY_SPACE))
        {
            upmove += 1;
        }
        
        if (input.isKeyPressed(GLFW_KEY_C))
        {
            upmove -= 1;
        }
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
    
    ma_engine* engine = AudioSystem::Get().GetEngine();
    ma_engine_listener_set_position(engine, 0, position.x, position.y, position.z);
    ma_engine_listener_set_direction(engine, 0, m_pCamera->getForward().x, m_pCamera->getForward().y, m_pCamera->getForward().z);
    ma_engine_listener_set_world_up(engine, 0, 0, 1, 0);
}
