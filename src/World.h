//
//  SourceMapScene.h
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 12.05.25.
//

#pragma once

#include <string>
#include <vector>
#include <memory>


class Viewport;

class Entity;
class Component;

class WorldScene
{
public:
    WorldScene();
    ~WorldScene();
    
    void loadMap(const std::string &filename);
    void update(float dt);
    void draw();
    
    void startSimulation(Viewport* viewport);
    void stopSimulation();
    
    void selectEntityAtIndex(int index);
    
    void save();
    void load();
    
    void registerComponent(Component* component);
    
    void queueComponentDestruction(Component* component) {
        m_componentsToDestroy.push_back(component);
    }
    
    void queueEntityDestruction(Entity* entity)
    {
        // Проверяем, нет ли его уже в очереди
        if (std::find(m_entitiesToDestroy.begin(), m_entitiesToDestroy.end(), entity) == m_entitiesToDestroy.end()) {
            m_entitiesToDestroy.push_back(entity);
        }
    }
    
    Viewport* m_pAttachedViewport;
    
    std::vector<std::unique_ptr<Entity>> m_entities;
    
    Entity* findEntityByName(const std::string& name);
    
    bool m_drawGrid = true;
    bool m_drawSkybox = true;
    
    bool m_isSimulationMode = false;
    
private:
    std::vector<Component*> m_globalUpdatables;
    std::vector<Component*> m_globalFixedUpdatables;
    std::vector<Component*> m_globalWaitingForStart;
    
    void processPendingStarts();
    void cleanupDeadComponents();
    void cleanupDeadEntities();
    
    void unregisterComponent(Component* component);
    
    std::vector<Component*> m_componentsToDestroy;
    std::vector<Entity*> m_entitiesToDestroy;
};

inline WorldScene* g_currentScene;
