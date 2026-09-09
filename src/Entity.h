//
//  SourceMapScene.h
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 12.05.25.
//

#pragma once

#include "Component.h"
#include "World.h"

#include <string>
#include <memory>
#include <unordered_map>

#include <glm/glm.hpp>

class Entity
{
public:
    Entity(const std::string &name);
    ~Entity();
    
    std::string m_name;
    bool m_isActive = true;
    
    // Добавление компонента
    template<typename T, typename... TArgs>
    T* addComponent(TArgs&&... args)
    {
        auto newComponent = std::make_unique<T>(std::forward<TArgs>(args)...);
        T* rawPtr = newComponent.get();
        
        rawPtr->owner = this;
        
        m_components.push_back(std::move(newComponent));
        m_componentMap.insert({typeid(T), rawPtr});
        
        g_currentScene->registerComponent(rawPtr);
        
        rawPtr->awake();
        return rawPtr;
    }
    
    // Получение ПЕРВОГО попавшегося компонента типа
    template<typename T>
    T* getComponent() const
    {
        auto it = m_componentMap.find(typeid(T));
        return (it != m_componentMap.end()) ? static_cast<T*>(it->second) : nullptr;
    }
    
    // Получение ВСЕХ компонентов определенного типа
    template <typename T>
    void getComponents(std::vector<T*>& outResults) const
    {
        auto range = m_componentMap.equal_range(typeid(T));
        
        for (auto it = range.first; it != range.second; ++it) {
            outResults.push_back(static_cast<T*>(it->second));
        }
    }
    
    // Получение ВСЕХ компонентов определенного типа
    template <typename T>
    std::vector<T*> getComponents() const
    {
        std::vector<T*> result;
        auto range = m_componentMap.equal_range(typeid(T));
        
        for (auto it = range.first; it != range.second; ++it) {
            result.push_back(static_cast<T*>(it->second));
        }
        return result;
    }
    
    const std::vector<std::unique_ptr<Component>>& getComponents() const { return m_components; }
    const bool isPendingKill() const { return m_isPendingKill; }
    const std::string& GetName() const { return m_name; }
    
    // Помечает компонент на удаление
    void destroyComponent(Component* component);
    
    // ВНУТРЕННИЙ МЕТОД: вызывается ТОЛЬКО Сценой в конце кадра для очистки памяти
    void freeComponentMemory(Component* component);
    
    void destroy();
    
private:
    std::vector<std::unique_ptr<Component>> m_components;
    std::unordered_multimap<std::type_index, Component*> m_componentMap;
    bool m_isPendingKill = false;
};
