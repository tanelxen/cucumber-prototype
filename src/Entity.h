//
//  SourceMapScene.h
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 12.05.25.
//

#pragma once

#include "TransformComponent.h"
#include "ExposedProperty.h"
#include "World.h"

#include "UID.h"

#include <string>
#include <memory>
#include <unordered_map>

#include <glm/glm.hpp>

class Entity
{
public:
    Entity(const std::string &name);
    ~Entity();
    
    void awake();
    
    std::string m_name;
    UID uid;
    
    bool m_isActive = true;
    
    WorldScene* scene = nullptr;
    
    Entity* parent = nullptr;
    std::vector<Entity*> children;
    
    std::string getName() const { return m_name; }
    Entity* getParent() const { return parent; }
    std::vector<Entity*> getChildren() const { return children; }
    
    // Для получения сущностей внутри префаба в стиле Godot
    Entity* findEntityByPath(const std::string& path) const;
    std::string getPathRelativeTo(const Entity* root) const;
    
    void addImpulse(const glm::vec3& force, const glm::vec3& point);
    
    // Добавление компонента
    template<typename T, typename... TArgs>
    T* addComponent(TArgs&&... args)
    {
        auto newComponent = std::make_unique<T>(std::forward<TArgs>(args)...);
        T* rawPtr = newComponent.get();
        
        rawPtr->owner = this;
        
        m_components.push_back(std::move(newComponent));
        m_componentMap.insert({typeid(T), rawPtr});
        
        scene->registerComponent(rawPtr);
        
        return rawPtr;
    }
    
    // Получение ПЕРВОГО попавшегося компонента типа
//    template<typename T>
//    T* getComponent() const
//    {
//        auto it = m_componentMap.find(typeid(T));
//        return (it != m_componentMap.end()) ? static_cast<T*>(it->second) : nullptr;
//    }
    
    template<typename T>
    T* getComponent()
    {
        if constexpr (std::same_as<T, TransformComponent>) {
            return &m_transform;
        }
        
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
    
    Component* getComponentByName(const std::string& typeName)
    {
        if (typeName == m_transform.getTypeName())
        {
            return &m_transform;
        }
        
        auto it = std::find_if(m_components.begin(), m_components.end(),
                               [&typeName](const std::unique_ptr<Component>& comp) {
            return comp && comp->getTypeName() == typeName;
        });
        
        return (it != m_components.end()) ? it->get() : nullptr;
    }
    
    TransformComponent& getTransform() { return m_transform; }
    const TransformComponent& getTransform() const { return m_transform; }
    TransformComponent* getTransformPtr() const { return (TransformComponent*)&m_transform; }
    
    void setParent(Entity* newParent, bool keepRelative);
    void invalidateTransform();
    
    const std::vector<std::unique_ptr<Component>>& getComponents() const { return m_components; }
    const bool isPendingKill() const { return m_isPendingKill; }
    const std::string& GetName() const { return m_name; }
    
    // Помечает компонент на удаление
    void destroyComponent(Component* component);
    
    // ВНУТРЕННИЙ МЕТОД: вызывается ТОЛЬКО Сценой в конце кадра для очистки памяти
    void freeComponentMemory(Component* component);
    
    void destroy();
    
public:
    // Связь с префабом
    std::string prefabAssetPath = "";
    bool isPrefabRoot = false;
    
    UID originalPrefabUid = UID::Invalid();
    
    bool isPrefabInstance() const { return !prefabAssetPath.empty(); }
    bool isPrefabChild() const { return isPrefabInstance() && !isPrefabRoot; }
    
    // нужно для diff
    nlohmann::json serializeToSnapshot() const;
    
public:
    // Кэш внешнего интерфейса (заполняется только у корня префаба)
    std::vector<CachedExposedProperty> m_exposedInterfaceCache;
    
    // Вспомогательная мапа для быстрого отслеживания оверрайдов на сцене: [ИмяСвойства -> АктивноЛи]
    // Заменяет сложную логику флагов Property<T> на уровне внешней сцены!
    std::unordered_map<std::string, bool> m_interfaceOverridesFlags;
    
    void markInterfaceOverrideDirty(const std::string& displayName, bool isDirty = true) {
        m_interfaceOverridesFlags[displayName] = isDirty;
    }
    
    bool isInterfaceOverrideDirty(const std::string& displayName) const {
        auto it = m_interfaceOverridesFlags.find(displayName);
        return it != m_interfaceOverridesFlags.end() ? it->second : false;
    }
    
private:
    std::vector<std::unique_ptr<Component>> m_components;
    std::unordered_multimap<std::type_index, Component*> m_componentMap;
    bool m_isPendingKill = false;
    
    TransformComponent m_transform;
};
