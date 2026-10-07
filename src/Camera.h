//
// Created by Fedor Artemenkov on 05.07.2024.
//

#pragma once
#include <glm/glm.hpp>

class Camera
{
public:
    Camera();
    
    void setTransform(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& right, const glm::vec3& up);
    void setAspectRatio(float aspectRatio);
    
    glm::vec3 getPosition() const;
    
    glm::vec3 getForward() const { return m_forward; }

    glm::mat4x4 projection;
    glm::mat4x4 view;
    
    float fov = 55.0f;

private:
    glm::vec3 m_position;

    glm::vec3 m_forward = {0, 0, 0};
    glm::vec3 m_right = {0, 0, 0};
    glm::vec3 m_up = {0, 0, 0};
    
    float width;
    float height;
};
