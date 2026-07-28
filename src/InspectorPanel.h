//
//  ViewportPanel.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#pragma once

class Mesh;

class InspectorPanel
{
public:
    InspectorPanel() = default;
    ~InspectorPanel() = default;
    
    void setEntity(Mesh* entity);
    void draw();
    
private:
    Mesh* m_entity;
};
