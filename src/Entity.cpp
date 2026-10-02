//
//  SourceMapScene.cpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 12.05.25.
//

#include "Entity.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#include <iostream>

//#define radians(deg) ((deg) * (M_PI / 180.0f))

Entity::Entity(const std::string& name) : m_name(std::move(name))
{
    m_transform.owner = this;
}

Entity::~Entity()
{
    std::cout << "~Entity(): " << m_name << std::endl;
}

void Entity::destroyComponent(Component* component)
{
    if (!component || component->isPendingKill) return;
    
    component->isPendingKill = true;
    component->enabled = false; // Сразу выключаем, чтобы он не апдейтился в этом кадре
    
    // Передаем Сцене задачу физически удалить его в конце кадра
    g_currentScene->queueComponentDestruction(component);
    
    // Удаляем из мапы быстрого поиска GetComponents
    auto range = m_componentMap.equal_range(typeid(*component));
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second == component) {
            m_componentMap.erase(it);
            break;
        }
    }
}

void Entity::awake()
{
    for (auto& component : m_components)
    {
        component->awake();
    }
}

void Entity::freeComponentMemory(Component* component)
{
    // 1. Удаляем из мапы быстрого поиска GetComponents
    auto range = m_componentMap.equal_range(typeid(*component));
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second == component) {
            m_componentMap.erase(it);
            break;
        }
    }
    
    // 2. Освобождаем unique_ptr из вектора
    m_components.erase(std::remove_if(m_components.begin(), m_components.end(),
                                      [component](const auto& comp) { return comp.get() == component; }),
                       m_components.end()
                       );
}

void Entity::destroy()
{
    if (m_isPendingKill) return;
    m_isPendingKill = true;
    
    for (auto& component : m_components) {
        destroyComponent(component.get());
    }
    
    // Ставим саму сущность в очередь Сцены на удаление в конце кадра
    g_currentScene->queueEntityDestruction(this);
}

void Entity::setParent(Entity* newParent, bool keepRelative)
{
    // 1. Запоминаем текущую МИРОВУЮ матрицу объекта, пока старая иерархия еще активна
    glm::mat4 oldWorldMatrix = m_transform.getWorldMatrix();
    
    // 2. Если у объекта уже был родитель, удаляем себя из его списка детей
    if (parent) {
        auto& pChildren = parent->children;
        pChildren.erase(std::remove(pChildren.begin(), pChildren.end(), this), pChildren.end());
    }
    
    parent = newParent;
    
    // 3. Добавляем себя новому родителю
    if (parent) {
        parent->children.push_back(this);
    }
    
    if (keepRelative)
    {
        // 4. ПЕРЕСЧЕТ ЛОКАЛЬНЫХ КООРДИНАТ
        if (parent)
        {
            // Получаем инвертированную мировую матрицу НОВОГО родителя
            glm::mat4 inverseParentMatrix = glm::inverse(parent->getTransform().getWorldMatrix());
            
            // Вычисляем новую локальную матрицу объекта относительно нового родителя
            glm::mat4 newLocalMatrix = inverseParentMatrix * oldWorldMatrix;
            
            // Раскладываем локальную матрицу на компоненты: position, rotation, scale
            glm::vec3 newLocalPos;
            glm::quat newLocalRot;
            glm::vec3 newLocalScale;
            glm::vec3 skew;                  // Временная переменная (не используется)
            glm::vec4 perspective;           // Временная переменная (не используется)
            
            glm::decompose(newLocalMatrix, newLocalScale, newLocalRot, newLocalPos, skew, perspective);
            
            // Применяем новые локальные трансформации
            // (Используем внутренние методы, которые не вызывают invalidateTransform повторно,
            // либо обновляем напрямую приватные поля Transform, если у GameObject есть к ним доступ)
            m_transform.setLocalPositionRaw(newLocalPos);
            m_transform.setLocalRotationRaw(newLocalRot);
            m_transform.setLocalScaleRaw(newLocalScale);
        }
        else
        {
            // Если объект вынесли в корень сцены, его локальные координаты становятся мировыми
            glm::vec3 newLocalPos;
            glm::quat newLocalRot;
            glm::vec3 newLocalScale;
            glm::vec3 skew;
            glm::vec4 perspective;
            
            glm::decompose(oldWorldMatrix, newLocalScale, newLocalRot, newLocalPos, skew, perspective);
            
            m_transform.setLocalPositionRaw(newLocalPos);
            m_transform.setLocalRotationRaw(newLocalRot);
            m_transform.setLocalScaleRaw(newLocalScale);
        }
    }
    
    // При смене родителя мировая матрица объекта (и его детей) гарантированно изменится
    invalidateTransform();
}

void Entity::invalidateTransform()
{
    auto& transform = getTransform();
    
    if (transform.isDirty()) return;
    transform.setDirty();
    
    // Быстро и без переборов всей сцены идем по указателям к детям
    for (Entity* child : children)
    {
        child->invalidateTransform();
    }
}

nlohmann::json Entity::serializeToSnapshot() const
{
    nlohmann::json json;
    json["name"] = m_name;
    
    auto componentsArray = nlohmann::json::array();
    
    {
        auto comp = getTransformPtr();
        
        nlohmann::json compNode;
        compNode["type"] = comp->getTypeName();
        
        nlohmann::json compData;
        comp->save(compData);
        compNode["data"] = compData;
        
        componentsArray.push_back(compNode);
    }
    
    for (auto& comp : m_components)
    {
        nlohmann::json compJson;
        compJson["type"] = comp->getTypeName();
        
        nlohmann::json compData;
        comp->save(compData);
        compJson["data"] = compData;
        
        componentsArray.push_back(compJson);
    }
    json["components"] = componentsArray;
    return json;
}
