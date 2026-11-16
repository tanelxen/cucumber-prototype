//
//  HierarchyPanel.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 10.09.26.
//

#pragma once

#include "EditorPanel.h"
#include <unordered_set>
#include <utility>
#include <vector>

class Entity;
class WorldScene;

class HierarchyPanel : public EditorPanel
{
public:
    HierarchyPanel();
    void draw() override;
    void setEntity(Entity* entity) { m_selectedEntity = entity; }
    
private:
    void DrawEntityNode(Entity* entity);
    
    bool IsChildOf(Entity* potentialChild, Entity* potentialParent) const;

    Entity* m_selectedEntity = nullptr;
    WorldScene* m_scene = nullptr;
    std::vector<Entity*> m_rootEntities;
    std::vector<std::pair<Entity*, Entity*>> m_pendingReparents;
    std::vector<Entity*> m_pendingDestructions;
    std::unordered_set<Entity*> m_liveEntities;
    std::unordered_set<Entity*> m_visitedEntities;
};
