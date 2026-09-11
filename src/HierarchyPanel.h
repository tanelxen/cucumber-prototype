//
//  HierarchyPanel.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 10.09.26.
//

#pragma once

#include "EditorPanel.h"
#include <vector>

class Entity;

class HierarchyPanel: public EditorPanel
{
public:
    HierarchyPanel();
    void draw() override;
    
    // Главный метод отрисовки окна
    void Draw(const std::vector<Entity*>& rootEntities, Entity*& selectedEntity);
    
private:
    // Рекурсивный метод для отрисовки дерева объектов
    void DrawEntityNode(Entity* entity, Entity*& selectedEntity);
    
    // Проверка: является ли node потенциальным ребенком parent (защита от зацикливания иерархии)
    bool IsChildOf(Entity* potentialChild, Entity* potentialParent);
};

