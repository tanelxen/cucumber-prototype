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
    
    void setScene(WorldScene* scene);
    void setEntity(Entity* entity);
    void setMaterial(Material* material);
    
    void startPicking(std::function<void(Entity*)> callback);
    void endPicking();
    
    void update(float dt);
    void draw();
    
    bool m_is_picking_mode = false;
    std::function<void(Entity*)> m_active_picking_callback = nullptr;
//    Entity* m_selected_entity = nullptr;
    
private:
    ViewportPanel* m_pViewportPanel;
    Viewport* m_pMainView;
    
    ScenePanel* m_pScenePanel;
    InspectorPanel* m_pInspectorPanel;
    AssetsPanel* m_pAssetsPanel;
    MaterialPanel* m_pMaterialPanel;
    
private:
    void drawMenuBar();
    void drawPanels();
    
    std::vector<EditorPanel*> m_panels;
};

inline UIManager* g_UIManager;
