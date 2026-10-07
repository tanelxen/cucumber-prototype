//
//  UIManager.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 08.08.26.
//

#pragma once

#include <vector>
#include <functional>

class EditorPanel;
class ViewportPanel;
class InspectorPanel;
class AssetsPanel;
class ScenePanel;
class HierarchyPanel;
class MaterialPanel;
class Viewport;

class WorldScene;
class Material;
class Entity;

class UIManager
{
public:
    UIManager();
    ~UIManager();
    
    void init(struct GLFWwindow* window);
    
    void setEntity(Entity* entity);
    void setMaterial(Material* material);
    
    Viewport* getMainViewport() { return m_pMainView; }
    
    void startPicking(std::function<void(Entity*)> callback);
    void endPicking();
    
    void update(float dt);
    void preRender();
    void draw();
    
    bool m_is_picking_mode = false;
    std::function<void(Entity*)> m_active_picking_callback = nullptr;
    
    ViewportPanel* m_pViewportPanel;
    Viewport* m_pMainView;
    
private:
    
    
    ScenePanel* m_pScenePanel;
    InspectorPanel* m_pInspectorPanel;
    AssetsPanel* m_pAssetsPanel;
    MaterialPanel* m_pMaterialPanel;
    HierarchyPanel* m_pHierarchyPanel;
    
private:
    void drawMenuBar();
    void drawPanels();
    
    std::vector<EditorPanel*> m_panels;
};

inline UIManager* g_UIManager;
