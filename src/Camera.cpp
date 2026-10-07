//
// Created by Fedor Artemenkov on 05.07.2024.
//

#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>

Camera::Camera()
{
    setAspectRatio(1);
}

void Camera::setAspectRatio(float aspectRatio)
{
    projection = glm::perspective(glm::radians(fov), aspectRatio, 0.1f, 8192.0f);
}

void Camera::setTransform(const glm::vec3 &position, const glm::vec3 &forward, const glm::vec3 &right, const glm::vec3 &up)
{
    m_position = position;
    m_forward = forward;
    m_right = right;
    m_up = up;
    
    view = glm::lookAt(position, position + forward, up);
    m_position = position;
}

glm::vec3 Camera::getPosition() const
{
    return m_position;
}


