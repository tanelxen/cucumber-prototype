//
//  WorldScene.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 12.05.25.
//

#pragma once

#include "UID.h"

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>

class Viewport;

class Entity;
class Component;
class CollisionComponent;
class CameraComponent;
class RenderView;
class Camera;

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
    
    void loadBSP(const std::string &filename);
    void update(float dt);
    
    void prepareRenderViews();
    void collectRenderCommands(RenderView& view);
    
    bool isSimulationMode() { return m_isSimulationMode; }
    void startSimulation(Viewport* viewport);
    void stopSimulation();
    
    void clear();
    void awake();
    
    void registerComponent(Component* component);
    
    void queueComponentDestruction(Component* component);
    void queueEntityDestruction(Entity* entity);
    
    void trace(struct HitResult& result,
               const glm::vec3 &start,
               const glm::vec3 &end,
               const glm::vec3 &mins,
               const glm::vec3 &maxs,
               Entity* ignore,
               uint32_t mask = 0xFFFFFFFF) const;
    
    // Открыто для удобства редактора
    std::vector<std::unique_ptr<Entity>> m_entities;
    
    Entity* findEntityByName(const std::string& name);
    Entity* findEntityByUID(const UID& uid);
    
    std::vector<CameraComponent*> getCameraComponents();
    
public:
    SceneType m_type = SceneType::Level; // По умолчанию сцена — это уровень
    std::string m_filePath = "";         // Путь к файлу на диске
    
    bool isPrefabContext() const { return m_type == SceneType::Prefab; }
    
public:
    void switchCamera(CameraComponent* newCameraComponent);
    
    bool hasActiveSceneCamera() const {
        // Возвращает true, если мы в симуляции и нашли хоть одну рабочую камеру на сцене
        return m_isSimulationMode && m_pActiveCameraComponent != nullptr;
    }
    
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
    
    Viewport* m_pGameplayViewport = nullptr; // Храним только на время симуляции
    CameraComponent* m_pActiveCameraComponent = nullptr; // Указатель на текущий компонент
};
