//
//  ViewportPanel.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#pragma once

class ShadedCube;

class InspectorPanel
{
public:
    InspectorPanel() = default;
    ~InspectorPanel() = default;
    
    void setEntity(ShadedCube* entity);
    void draw();
    
private:
    ShadedCube* m_entity;
};
