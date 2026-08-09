//
//  UIManager.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 08.08.26.
//

#pragma once

class ViewportPanel;
class InspectorPanel;
class AssetsPanel;
class ScenePanel;
class Viewport;

class WorldScene;

class UIManager
{
public:
    UIManager();
    ~UIManager();
    
    void init(struct GLFWwindow* window);
    
    void setScene(WorldScene* scene);
    
    void update(float dt);
    void draw();
    
public:
    
    ViewportPanel* m_pViewportPanel;
    Viewport* m_pMainView;
    
    ScenePanel* m_pScenePanel;
    InspectorPanel* m_pInspectorPanel;
    AssetsPanel* m_pAssetsPanel;
    
private:
    void drawMenuBar();
    void drawPanels();
};
