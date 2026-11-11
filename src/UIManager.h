//
//  UIManager.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 08.08.26.
//

#pragma once

#include "IViewProvider.h"
#include "level/LevelWireframe.h"

#include <vector>
#include <filesystem>
#include <string>
#include <glm/glm.hpp>

class EditorPanel;
class ViewportPanel;
class InspectorPanel;
class AssetsPanel;
class HierarchyPanel;
class EditorCamera;
class TexturingPanel;
class TextureBrowserPanel;

class WorldScene;
struct ViewDesc;

class Material;
class Entity;

class UIManager : public IViewProvider
{
public:
    UIManager();
    ~UIManager();
    
    void init(struct GLFWwindow* window);
    
    void setEntity(Entity* entity);
    void setMaterial(Material* material);
    void drawEntityCreationMenu(WorldScene& scene, const glm::vec3& position);
    
    void showTextureBrowser();
    
    void startFrame();
    void update(float dt);
    void draw();
    EditorCamera& editorCamera();
    void resetEditorCamera();
    
    ViewportPanel* m_pViewportPanel;
    
    void collectViews(WorldScene& scene, std::vector<ViewDesc>& out) override;
    
    float gridSize = 16.0f;
    
private:
    InspectorPanel* m_pInspectorPanel;
    AssetsPanel* m_pAssetsPanel;
    HierarchyPanel* m_pHierarchyPanel;
    TexturingPanel* m_pTexturingPanel;
    TextureBrowserPanel* m_pTextureBrowserPanel;
    
private:
    void drawMenuBar();
    void drawPanels();
    void drawProjectLauncher();
    bool openProjectPath(const std::filesystem::path& path);
    
    std::vector<ViewportPanel*> m_viewportPanels;
    std::vector<EditorPanel*> m_panels;
    
    bool m_showProjectLauncher = true;
    char m_projectNameBuffer[128] = "MyProject";
    char m_projectLocationBuffer[1024] = {};
    std::string m_projectError;
    
    LevelWireframe m_levelWireframe;
};

inline UIManager* g_UIManager;
