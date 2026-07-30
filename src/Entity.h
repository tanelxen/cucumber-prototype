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

struct Component { virtual ~Component() = default; };

struct TransformComponent : Component
{
    glm::vec3 position = { 0.0f, 0.0f, 0.0f };
    glm::vec3 rotation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
};

struct MeshComponent : Component
{
    std::string path = "<no mesh>";
    glm::vec4 tintColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    Mesh mesh;
};

class Entity
{
public:
    Entity(const std::string &name);
    
    std::string m_name;
    
    template<typename T, typename... Args>
    void addComponent(Args&&... args)
    {
        m_components[typeid(T)] = std::make_unique<T>(std::forward<Args>(args)...);
    }
    
    template<typename T>
    T* getComponent()
    {
        auto it = m_components.find(typeid(T));
        if (it != m_components.end()) {
            return static_cast<T*>(it->second.get());
        }
        return nullptr;
    }
    
//private:
    // Ключ — тип компонента, значение — сам компонент
    std::unordered_map<std::type_index, std::unique_ptr<Component>> m_components;
};
