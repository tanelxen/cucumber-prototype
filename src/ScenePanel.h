//
//  ViewportPanel.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#pragma once

#include "EditorPanel.h"

class WorldScene;

class ScenePanel: public EditorPanel
{
public:
    ScenePanel();
    ~ScenePanel() = default;
    
    void setScene(WorldScene* scene);
    void draw() override;
    
private:
    WorldScene* m_scene;
};
