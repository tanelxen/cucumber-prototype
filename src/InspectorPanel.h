//
//  ViewportPanel.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#pragma once

#include "EditorPanel.h"

class Entity;
struct ImVec2;

class InspectorPanel: public EditorPanel
{
public:
    InspectorPanel();
    ~InspectorPanel() = default;
    
    void setEntity(Entity* entity);
    void draw() override;
    
private:
    Entity* m_entity;
    
    void DrawInspector(Entity* selectedEntity);
    void DrawNormalEntityComponents(Entity* entity);
    void DrawPrefabInstanceInspector(Entity* prefabRoot);
    
    bool RenderAssetDropZone(const char* str_id, const ImVec2& size, float targetLineY);
};
