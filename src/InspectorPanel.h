//
//  ViewportPanel.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#pragma once

#include "EditorPanel.h"

class Entity;

class InspectorPanel: public EditorPanel
{
public:
    InspectorPanel();
    ~InspectorPanel() = default;
    
    void setEntity(Entity* entity);
    void draw() override;
    
private:
    Entity* m_entity;
};
