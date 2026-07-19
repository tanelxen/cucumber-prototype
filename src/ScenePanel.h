//
//  ViewportPanel.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#pragma once

#include <string>

class WorldScene;

class ScenePanel
{
public:
    ScenePanel() = default;
    ~ScenePanel() = default;
    
    void setScene(WorldScene* scene);
    void draw();
    
private:
    WorldScene* m_scene;
};
