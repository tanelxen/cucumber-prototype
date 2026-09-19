//
//  TransformComponent.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 10.09.26.
//

#include "TransformComponent.h"
#include "Entity.h"

#include <glm/gtc/matrix_transform.hpp>

glm::mat4 TransformComponent::toMat4()
{
    glm::mat4 localMatrix = getLocalMat4();
    
    // Если у нашей сущности есть родитель, умножаем его мировую матрицу на нашу локальную
    if (owner && owner->parent)
    {
        if (auto* parentTransform = owner->parent->getComponent<TransformComponent>())
        {
            return parentTransform->toMat4() * localMatrix;
        }
    }
    
    return localMatrix;
}

glm::mat4 TransformComponent::getLocalMat4() const
{
    glm::mat4 model(1.0f);
    
    model = glm::translate(model, position);
    
    model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0, 1, 0));
    model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1, 0, 0));
    model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0, 0, 1));
    
    model = glm::scale(model, scale);
    
    return model;
}

void TransformComponent::save(nlohmann::json& j) const
{
    j = nlohmann::json {
        {"position", {position.x, position.y, position.z}},
        {"rotation", {rotation.x, rotation.y, rotation.z}},
        {"scale",    {scale.x,    scale.y,    scale.z}}
    };
}

void TransformComponent::load(const nlohmann::json& j)
{
    auto pos = j.at("position").get<std::vector<float>>();
    position = glm::vec3(pos[0], pos[1], pos[2]);
    
    auto rot = j.at("rotation").get<std::vector<float>>();
    rotation = glm::vec3(rot[0], rot[1], rot[2]);
    
    auto scl = j.at("scale").get<std::vector<float>>();
    scale = glm::vec3(scl[0], scl[1], scl[2]);
}

glm::vec3 TransformComponent::getForward()
{
//    // Переводим углы из градусов в радианы и создаем матрицу поворота
//    glm::vec3 rads = glm::radians(rotation);
//    glm::mat4 transform(1.0f);
//    
//    // Порядок применения вращений важен (например, Yaw -> Pitch -> Roll или YXZ)
//    transform = glm::rotate(transform, rads.y, glm::vec3(0.0f, 1.0f, 0.0f)); // Yaw (Y)
//    transform = glm::rotate(transform, rads.x, glm::vec3(1.0f, 0.0f, 0.0f)); // Pitch (X)
//    transform = glm::rotate(transform, rads.z, glm::vec3(0.0f, 0.0f, 1.0f)); // Roll (Z)
//    
//    // Извлекаем вектора из столбцов матрицы (для правой системы координат OpenGL)
//    // Если ваш базовый forward в покое смотрит по -Z:
//    return glm::normalize(glm::vec3(transform * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
    
    float pitch = glm::radians(rotation.x);
    float yaw   = glm::radians(rotation.y);
    
    // Корректный расчет forward для стандартной камеры OpenGL (взгляд изначально вдоль -Z)
    glm::vec3 forward;
    forward.x = -sin(yaw) * cos(pitch);
    forward.y =  sin(pitch);
    forward.z = -cos(yaw) * cos(pitch);
    
    return glm::normalize(forward);
}

glm::vec3 TransformComponent::getRight()
{
//    // Переводим углы из градусов в радианы и создаем матрицу поворота
//    glm::vec3 rads = glm::radians(rotation);
//    glm::mat4 transform(1.0f);
//    
//    // Порядок применения вращений важен (например, Yaw -> Pitch -> Roll или YXZ)
//    transform = glm::rotate(transform, rads.y, glm::vec3(0.0f, 1.0f, 0.0f)); // Yaw (Y)
//    transform = glm::rotate(transform, rads.x, glm::vec3(1.0f, 0.0f, 0.0f)); // Pitch (X)
//    transform = glm::rotate(transform, rads.z, glm::vec3(0.0f, 0.0f, 1.0f)); // Roll (Z)
//    
//    // Извлекаем вектора из столбцов матрицы (для правой системы координат OpenGL)
//    // Если ваш базовый forward в покое смотрит по -Z:
////    return glm::normalize(glm::vec3(transform * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
//    return glm::normalize(glm::vec3(transform * glm::vec4(1.0f, 0.0f,  0.0f, 0.0f)));
    
    float pitch = glm::radians(rotation.x);
    float yaw   = glm::radians(rotation.y);
    
    // Корректный расчет right (взгляд изначально вдоль +X)
    glm::vec3 right;
    right.x =  cos(yaw);
    right.y =  0.0f;
    right.z = -sin(yaw);
    
    return glm::normalize(right);
}

