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
#include "../vendor/json.hpp"

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
    
    virtual void save(nlohmann::json& j) const = 0;
    virtual void load(const nlohmann::json& j) = 0;
    virtual const std::string getTypeName() const = 0;
    
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
    
    const std::string getTypeName() const override { return "TransformComponent"; }
    
    void save(nlohmann::json& j) const override
    {
        j = nlohmann::json {
            {"position", {position.x, position.y, position.z}},
            {"rotation", {rotation.x, rotation.y, rotation.z}},
            {"scale",    {scale.x,    scale.y,    scale.z}}
        };
    }
    
    void load(const nlohmann::json& j) override
    {
        auto pos = j.at("position").get<std::vector<float>>();
        position = glm::vec3(pos[0], pos[1], pos[2]);
        
        auto rot = j.at("rotation").get<std::vector<float>>();
        rotation = glm::vec3(rot[0], rot[1], rot[2]);
        
        auto scl = j.at("scale").get<std::vector<float>>();
        scale = glm::vec3(scl[0], scl[1], scl[2]);
    }
};

struct MeshComponent : Component
{
    std::string path = "<no mesh>";
    
    std::shared_ptr<Mesh> mesh;
    std::vector<std::shared_ptr<Material>> materials;
    
    const std::string getTypeName() const override { return "MeshComponent"; }
    
    void save(nlohmann::json& j) const override
    {
        j = nlohmann::json{ {"source_glb", path} };
    }
    
    void load(const nlohmann::json& j) override
    {
        path = j.at("source_glb");
    }
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
    
    const std::vector<std::unique_ptr<Component>>& getAllComponents() const { return m_components; }
    
private:
    std::vector<std::unique_ptr<Component>> m_components;
    std::unordered_map<std::type_index, Component*> m_componentMap;
};
