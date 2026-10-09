//
//  CameraController.h
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 03.11.24.
//

#pragma once

#include <glm/glm.hpp>
#include "Input.h"

class Camera;

class CameraController
{
public:
    CameraController(Camera* camera);
    
    void update(float dt, const Input::State& input);
    
    glm::vec3 position = {0, 0, 0};
    
    glm::vec3 forward = {0, 0, 0};
    glm::vec3 right = {0, 0, 0};
    glm::vec3 up = {0, 0, 0};
    
    float pitch = 0;
    float yaw = 0;
    
    bool isFlyMode() { return m_isFlyMode; }

private:
    Camera* m_pCamera;
    bool m_isFlyMode = false;
};
