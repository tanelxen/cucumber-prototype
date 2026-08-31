//
//  SourceMapScene.h
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 12.05.25.
//

#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <typeindex>

#include <glm/glm.hpp>

#include "Mesh.h"
//#include "Animator.h"

//struct Transform
//{
//    glm::vec3 position = { 0.0f, 0.0f, 0.0f };
//    glm::vec3 rotation = { 0.0f, 0.0f, 0.0f };
//    glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
//    
//    glm::mat4 toMat4();
//};

class Entity;

class Component {
public:
    Entity* owner = nullptr;
    bool enabled = true;
    
    virtual ~Component() = default;
    virtual void awake() {}
    virtual void update(float dt) {}
    virtual void onImGuiRender() {}
    
    virtual std::type_index GetTypeIndex() const {
        return std::type_index(typeid(*this));
    }
};

struct TransformComponent : Component
{
    glm::vec3 position = { 0.0f, 0.0f, 0.0f };
    glm::vec3 rotation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
    
    glm::mat4 toMat4();
};

struct MeshComponent : Component
{
    std::string path = "<no mesh>";
    
    std::shared_ptr<Mesh> mesh;
    std::vector<std::shared_ptr<Material>> materials;
//    Animator animator;
};

class Entity
{
public:
    Entity(const std::string &name);
    
    std::string m_name;
    bool m_isActive;
    
//    const std::string& getName() const { return m_name; }
    const std::vector<std::unique_ptr<Component>>& getComponents() const { return m_components; }
    
    // Добавление компонента
    template<typename T, typename... TArgs>
    T* addComponent(TArgs&&... args)
    {
        auto newComponent = std::make_unique<T>(std::forward<TArgs>(args)...);
        T* componentPtr = newComponent.get();
        
        componentPtr->owner = this;
        m_components.push_back(std::move(newComponent));
        m_componentMap[typeid(T)] = componentPtr;
        
        componentPtr->awake();
        return componentPtr;
    }
    
    // Получение компонента
    template<typename T>
    T* getComponent()
    {
        auto it = m_componentMap.find(typeid(T));
        return (it != m_componentMap.end()) ? static_cast<T*>(it->second) : nullptr;
    }
    
    // Обновление всех компонентов сущности
    void update(float dt)
    {
        for (auto& component : m_components) {
            if (component->enabled) {
                component->update(dt);
            }
        }
    }
    
private:
    std::vector<std::unique_ptr<Component>> m_components;
    std::unordered_map<std::type_index, Component*> m_componentMap;
};
