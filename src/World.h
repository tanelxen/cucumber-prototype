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
#include <glm/glm.hpp>

class Viewport;

class Entity;
class Component;

class WorldScene
{
public:
    WorldScene();
    ~WorldScene();
    
    void loadBSP(const std::string &filename);
    void update(float dt);
    void draw();
    
    void startSimulation(Viewport* viewport);
    void stopSimulation();
    
    void selectEntityAtIndex(int index);
    
    void save(const std::string& filename);
    void load(const std::string& filename);
    
    void registerComponent(Component* component);
    
    void queueComponentDestruction(Component* component);
    void queueEntityDestruction(Entity* entity);
    
    void trace(struct HitResult& result, const glm::vec3 &start, const glm::vec3 &end, const glm::vec3 &mins, const glm::vec3 &maxs) const;
    
    Viewport* m_pAttachedViewport;
    
    // Открыто для удобства редактора
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
