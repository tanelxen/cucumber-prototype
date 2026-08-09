//
//  UIManager.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 08.08.26.
//

#pragma once

#include <vector>

class EditorPanel;
class ViewportPanel;
class InspectorPanel;
class AssetsPanel;
class ScenePanel;
class Viewport;

class WorldScene;
class Entity;

class UIManager
{
public:
    UIManager();
    ~UIManager();
    
    void init(struct GLFWwindow* window);
    
    void setScene(WorldScene* scene);
    void setEntity(Entity* entity);
    
    void update(float dt);
    void draw();
    
private:
    ViewportPanel* m_pViewportPanel;
    Viewport* m_pMainView;
    
    ScenePanel* m_pScenePanel;
    InspectorPanel* m_pInspectorPanel;
    AssetsPanel* m_pAssetsPanel;
    
private:
    void drawMenuBar();
    void drawPanels();
    
    std::vector<EditorPanel*> m_panels;
};
