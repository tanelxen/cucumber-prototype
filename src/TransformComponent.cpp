//
//  TransformComponent.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 10.09.26.
//

#include "TransformComponent.h"
#include "Entity.h"

#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

void TransformComponent::save(nlohmann::json& j) const
{
    j = nlohmann::json {
        {"position", {m_position.x, m_position.y,   m_position.z}},
        {"rotation", {m_euler.x,    m_euler.y,      m_euler.z}},
        {"scale",    {m_scale.x,    m_scale.y,      m_scale.z}}
    };
}

void TransformComponent::load(const nlohmann::json& j)
{
    auto pos = j.at("position").get<std::vector<float>>();
    m_position = glm::vec3(pos[0], pos[1], pos[2]);
    
    auto rot = j.at("rotation").get<std::vector<float>>();
    m_euler = glm::vec3(rot[0], rot[1], rot[2]);
    
    auto scl = j.at("scale").get<std::vector<float>>();
    m_scale = glm::vec3(scl[0], scl[1], scl[2]);
}

// Вектора направлений берутся из актуальной мировой матрицы
glm::vec3 TransformComponent::getForward()
{
    return glm::normalize(-glm::vec3(getWorldMatrix()[2]));
}

glm::vec3 TransformComponent::getRight()
{
    return glm::normalize(glm::vec3(getWorldMatrix()[0]));
}

glm::vec3 TransformComponent::getUp()
{
    return glm::normalize(glm::vec3(getWorldMatrix()[1]));
}

glm::mat4 TransformComponent::getWorldMatrix()
{
    if (m_isDirty)
    {
        updateLocalMatrix();
        
        if (owner->parent == nullptr)
        {
            // Если корней нет, мировая матрица равна локальной
            m_worldMatrix = m_localMatrix;
        }
        else
        {
            auto& parentTransform = owner->parent->getTransform();
            
            if (enabled)
            {
                // Рекурсивный подъем вверх к корню и расчет на обратном пути
                m_worldMatrix = parentTransform.getWorldMatrix() * m_localMatrix;
            }
            else
            {
                // Выключенный компонент становится транзитным
                m_worldMatrix = parentTransform.getWorldMatrix();
            }
        }
        
        m_isDirty = false;
    }
    
    return m_worldMatrix;
}

void TransformComponent::setWorldMatrix(const glm::mat4& newWorldMatrix)
{
    glm::mat4 newLocalMatrix = newWorldMatrix;
    
    // Если есть родитель, пересчитываем мировую матрицу в локальную
    Entity* parentNode = owner->parent;
    
    if (parentNode) {
        glm::mat4 inverseParentMatrix = glm::inverse(parentNode->getTransform().getWorldMatrix());
        newLocalMatrix = inverseParentMatrix * newWorldMatrix;
    }
    
    // Раскладываем локальную матрицу на компоненты
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(newLocalMatrix, m_scale, m_rotation, m_position, skew, perspective);
    
    // Обновляем сохраненные углы Эйлера для UI
    m_euler = glm::degrees(glm::eulerAngles(m_rotation));
    
    // Инвалидируем кэш для всех дочерних объектов
    owner->invalidateTransform();
}

void TransformComponent::updateLocalMatrix()
{
    m_localMatrix = glm::identity<glm::mat4>();
    
    if (enabled)
    {
        m_localMatrix = glm::translate(m_localMatrix, m_position);
        m_localMatrix = glm::rotate(m_localMatrix, glm::radians(m_euler.y), glm::vec3(0, 1, 0));
        m_localMatrix = glm::rotate(m_localMatrix, glm::radians(m_euler.x), glm::vec3(1, 0, 0));
        m_localMatrix = glm::rotate(m_localMatrix, glm::radians(m_euler.z), glm::vec3(0, 0, 1));
        m_localMatrix = glm::scale(m_localMatrix, m_scale);
    }
    
    // Для кватернионного m_rotation
    /*
     glm::mat4 translationM = glm::translate(glm::mat4(1.0f), m_position);
     glm::mat4 rotationM    = glm::toMat4(m_rotation);
     glm::mat4 scaleM       = glm::scale(glm::mat4(1.0f), m_scale);
     m_localMatrix = translationM * rotationM * scaleM;
     */
}

void TransformComponent::setPosition(const glm::vec3 &pos)
{
    m_position = pos;
    
    if (owner) {
        owner->invalidateTransform();
    }
}

glm::vec3 TransformComponent::getPosition() const
{
    return m_position;
}

void TransformComponent::setRotation(const glm::vec3 &eulerDegrees)
{
    m_euler = eulerDegrees;
    m_rotation = glm::quat(glm::radians(eulerDegrees));
    
    if (owner) {
        owner->invalidateTransform();
    }
}

glm::vec3 TransformComponent::getRotation() const
{
    return m_euler;
}

void TransformComponent::setScale(const glm::vec3 &scale)
{
    m_scale = scale;
    
    if (owner) {
        owner->invalidateTransform();
    }
}

glm::vec3 TransformComponent::getScale() const
{
    return m_scale;
}

