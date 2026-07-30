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

class ViewportPanel;
class ScenePanel;
class InspectorPanel;
class AssetsPanel;

class Viewport;

class Entity;

class WorldScene
{
public:
    WorldScene();
    ~WorldScene();
    
    void loadMap(const std::string &filename);
    void update(float dt);
    void draw();
    
    void selectEntityAtIndex(int index);
    
    Viewport* m_pMainView;
//    Viewport* m_pMainView2;
    
    ViewportPanel* m_pViewportPanel;
//    ViewportPanel* m_pViewportPanel2;
    ScenePanel* m_pScenePanel;
    InspectorPanel* m_pInspectorPanel;
    AssetsPanel* m_pAssetsPanel;
    
//    std::vector<std::unique_ptr<Entity>> m_entities;
    std::vector<std::unique_ptr<Entity>> m_entities;
    
private:
};
