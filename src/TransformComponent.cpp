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
        {"rotation", {m_euler.x,      m_euler.y,        m_euler.z}},
        {"scale",    {m_scale.x,    m_scale.y,      m_scale.z}}
    };
}

void TransformComponent::load(const nlohmann::json& j)
{
    if (j.contains("position"))
    {
        auto pos = j.at("position").get<std::vector<float>>();
        m_position = glm::vec3(pos[0], pos[1], pos[2]);
    }
    
    if (j.contains("rotation"))
    {
        auto rot = j.at("rotation").get<std::vector<float>>();
        m_euler = glm::vec3(rot[0], rot[1], rot[2]);
        m_rotation = glm::quat(glm::radians(m_euler));
    }
    
    if (j.contains("scale"))
    {
        auto scl = j.at("scale").get<std::vector<float>>();
        m_scale = glm::vec3(scl[0], scl[1], scl[2]);
    }
}

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
    glm::vec3 pos;
    glm::decompose(newLocalMatrix, m_scale, m_rotation, pos, skew, perspective);
    
    m_position = pos;
    m_euler = glm::degrees(glm::eulerAngles(m_rotation));
    
    // Инвалидируем кэш для всех дочерних объектов
    owner->invalidateTransform();
}

void TransformComponent::updateLocalMatrix()
{
    glm::mat4 translationM = glm::translate(glm::mat4(1.0f), m_position);
    glm::mat4 rotationM    = glm::mat4_cast(m_rotation);
    glm::mat4 scaleM       = glm::scale(glm::mat4(1.0f), m_scale);
    m_localMatrix = translationM * rotationM * scaleM;
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

glm::vec3 TransformComponent::getWorldPosition()
{
    return glm::vec3(getWorldMatrix()[3]);
}

void TransformComponent::setRotation(const glm::vec3 &eulerDegrees)
{
    m_rotation = glm::quat(glm::radians(eulerDegrees));
    m_euler = eulerDegrees;
    
    if (owner) {
        owner->invalidateTransform();
    }
}

void TransformComponent::setRotation(const glm::quat& rotation)
{
    m_rotation = rotation;
    m_euler = glm::degrees(glm::eulerAngles(m_rotation));
    
    if (owner) {
        owner->invalidateTransform();
    }
}

glm::vec3 TransformComponent::getRotation() const
{
    return m_euler;
}

glm::quat TransformComponent::getWorldRotation()
{
    glm::mat4 worldMatrix = getWorldMatrix();
    
    // Нормализуем колонки матрицы от масштабирования
    glm::mat3 rotMat;
    rotMat[0] = glm::normalize(glm::vec3(worldMatrix[0]));
    rotMat[1] = glm::normalize(glm::vec3(worldMatrix[1]));
    rotMat[2] = glm::normalize(glm::vec3(worldMatrix[2]));
    
    return glm::quat_cast(rotMat);
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

nlohmann::json TransformComponent::getExposedValue(const std::string& propName)
{
    glm::vec3 euler = glm::degrees(glm::eulerAngles(m_rotation));
    
    if (propName == "position") return { m_position.x,  m_position.y,   m_position.z    };
    if (propName == "rotation") return { euler.x,       euler.y,        euler.z         };
    if (propName == "scale")    return { m_scale.x,     m_scale.y,      m_scale.z       };
    return nlohmann::json();
}

void TransformComponent::setExposedValue(const std::string& propName, const nlohmann::json& value)
{
    if (propName == "position")
    {
        auto pos = value.get<std::vector<float>>();
        m_position = glm::vec3(pos[0], pos[1], pos[2]);
        
        if (owner) {
            owner->invalidateTransform();
        }
    }
    
    if (propName == "rotation")
    {
        auto rot = value.get<std::vector<float>>();
        
        glm::vec3 euler = glm::vec3(rot[0], rot[1], rot[2]);
        m_rotation = glm::quat(glm::radians(euler));
        
        if (owner) {
            owner->invalidateTransform();
        }
    }
    
    if (propName == "scale")
    {
        auto pos = value.get<std::vector<float>>();
        m_scale = glm::vec3(pos[0], pos[1], pos[2]);
        
        if (owner) {
            owner->invalidateTransform();
        }
    }
}

// ==================== PROXY TRANSLATE ======================
void TransformComponent::translate(const glm::vec3& delta)
{
    setPosition(m_position + delta);
}

void TransformComponent::translateX(float delta)
{
    m_position.x += delta;
    setPosition(m_position);
}

void TransformComponent::translateY(float delta)
{
    m_position.y += delta;
    setPosition(m_position);
}

void TransformComponent::translateZ(float delta)
{
    m_position.z += delta;
    setPosition(m_position);
}

// ===================== PROXY ROTATE =======================
void TransformComponent::rotate(const glm::vec3& delta)
{
    setRotation(m_euler + delta);
}

void TransformComponent::rotatePitch(float delta)
{
    m_euler.x += delta;
    setRotation(m_euler);
}

void TransformComponent::rotateYaw(float delta)
{
    m_euler.y += delta;
    setRotation(m_euler);
}

void TransformComponent::rotateRoll(float delta)
{
    m_euler.z += delta;
    setRotation(m_euler);
}
