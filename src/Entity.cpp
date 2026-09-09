//
//  SourceMapScene.cpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 12.05.25.
//

#include "Entity.h"
#include <iostream>

//#define radians(deg) ((deg) * (M_PI / 180.0f))

Entity::Entity(const std::string& name) : m_name(std::move(name))
{
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
