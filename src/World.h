//
//  WorldScene.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 12.05.25.
//

#pragma once

#include "UID.h"

#include "CameraManager.h"
#include "ViewDesc.h"

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>

class Viewport;

class Entity;
class Component;
class CollisionComponent;
class CameraComponent;
class MapComponent;

enum class SceneType
{
    Level,    // Обычный игровой уровень (.scene)
    Prefab    // Префаб в режиме изолированного редактирования исходника (.prefab)
};

class WorldScene
{
public:
    WorldScene();
    ~WorldScene();
    
    void update(float dt);
    
    bool isSimulationMode() { return m_isSimulationMode; }
    void startSimulation();
    void stopSimulation();
    
    void clear();
    void awake();
    
    void registerComponent(Component* component);
    
    void queueComponentDestruction(Component* component);
    void queueEntityDestruction(Entity* entity);
    
    void lineTrace(struct RaycastHit& outHit,
                   const glm::vec3& start,
                   const glm::vec3& direction,
                   float maxDistance,
                   Entity* ignoreEntity,
                   uint32_t mask = 0xFFFFFFFF) const;
    
    // Открыто для удобства редактора
    std::vector<std::unique_ptr<Entity>> m_entities;
    
    Entity* findEntityByName(const std::string& name);
    Entity* findEntityByUID(const UID& uid);
    
public:
    SceneType m_type = SceneType::Level; // По умолчанию сцена — это уровень
    std::string m_filePath = "";         // Путь к файлу на диске
    
    bool isPrefabContext() const { return m_type == SceneType::Prefab; }
    
public:
    CameraManager& cameras() { return m_cameras; }
    const CameraManager& cameras() const { return m_cameras; }
        
    // Оставлены как тонкие обёртки, чтобы не ломать скрипты/редактор:
    void switchCamera(CameraComponent* c)     { m_cameras.setPrimary(c); }
    void setPrimaryCamera(CameraComponent* c) { m_cameras.setPrimary(c); }
    std::vector<CameraComponent*> getCameraComponents() { return m_cameras.all(); }
    bool hasActiveSceneCamera() const { return m_isSimulationMode && m_cameras.hasActive(); }
    
    MapComponent* getMap() const { return m_map; }
    
private:
    std::vector<Component*> m_globalUpdatables;
    std::vector<Component*> m_allComponents;
    std::vector<CollisionComponent*> m_globalColliders;
    
    void cleanupDeadComponents();
    void cleanupDeadEntities();
    
    void unregisterComponent(Component* component);
    
    std::vector<Component*> m_componentsToDestroy;
    std::vector<Entity*> m_entitiesToDestroy;
    
    bool m_isSimulationMode = false;
    
    MapComponent* m_map = nullptr;
    
    CameraManager m_cameras;
};
