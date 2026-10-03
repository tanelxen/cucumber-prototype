//
//  TransformComponent.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 10.09.26.
//

#pragma once

#include "Component.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

struct TransformComponent : public Component
{
    // Возможно стоит делать обновление m_worldMatrix в конце кадра
    // Но тогда в скриптах это придется это учесть и дать метод Invalidate()
    // На случай когда тут же нужно свежее значение
    // Зато из скрипта можно будет прямо образаться к свойствам
    
//    glm::vec3 position = { 0.0f, 0.0f, 0.0f };
//    glm::vec3 rotation = { 0.0f, 0.0f, 0.0f };
//    glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
    
    const std::string getTypeName() const override { return "TransformComponent"; }
    
    void save(nlohmann::json& j) const override;
    void saveOverrides(nlohmann::json& outData) const override;
    void load(const nlohmann::json& j) override;
    
    void setPosition(const glm::vec3& pos);
    glm::vec3 getPosition() const;
    bool isPositionOverriden() { return m_position.isOverridden(); }
    
    glm::vec3 getWorldPosition();
    
    void setRotation(const glm::vec3& eulerDegrees);
    glm::vec3 getRotation() const;
    
    void setScale(const glm::vec3& scale);
    glm::vec3 getScale() const;
    
    glm::vec3 getForward();
    glm::vec3 getRight();
    glm::vec3 getUp();
    
    glm::mat4 getWorldMatrix();
    void setWorldMatrix(const glm::mat4& worldMatrix);
    
    bool isDirty() const { return m_isDirty; }
    void setDirty() { m_isDirty = true; }
    
public:
    // "Сырые" сеттеры для внутренних нужд движка (например, для setParent)
    void setLocalPositionRaw(const glm::vec3& pos) { m_position = pos; }
    void setLocalRotationRaw(const glm::quat& rot) {
        m_rotation = rot;
        m_euler = glm::degrees(glm::eulerAngles(rot));
    }
    void setLocalScaleRaw(const glm::vec3& scale)  { m_scale = scale;  }
    
private:
    mutable glm::mat4 m_localMatrix;
    mutable glm::mat4 m_worldMatrix;
    mutable bool m_isDirty = true;
    
    void updateLocalMatrix();
    
private:
    Property<glm::vec3> m_position = glm::vec3(0);
    glm::quat m_rotation = { 1.0f, 0.0f, 0.0f, 0.0f };
    glm::vec3 m_euler = { 0.0f, 0.0f, 0.0f };
    glm::vec3 m_scale = { 1.0f, 1.0f, 1.0f };
};
