//
//  CameraController.h
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 03.11.24.
//

#pragma once

#include <memory>
#include <glm/glm.hpp>

class Camera;

class CameraController
{
public:
    CameraController(Camera* camera);
    
    void update(float dt);
    
    glm::vec3 position = {0, 0, 0};
    
    glm::vec3 forward = {0, 0, 0};
    glm::vec3 right = {0, 0, 0};
    glm::vec3 up = {0, 0, 0};
    
    float pitch = -0.5;
    float yaw = -1.57;

private:
    Camera* m_pCamera;
};
