//
//  SourceMapScene.cpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 12.05.25.
//

#include "World.h"

#include "Camera.h"

#include "Cube.h"
#include "Grid.h"

#include "Viewport.h"

#include "ViewportPanel.h"
#include "ScenePanel.h"
#include "InspectorPanel.h"
#include "AssetsPanel.h"

#include "RenderDevice.h"

static ShadedCube g_shadedCube;
static Grid g_grid;

WorldScene::WorldScene()
{
    g_shadedCube.init();
    g_shadedCube.scale = { 64, 64, 64 };
    g_shadedCube.position = { 0, 0, -64 };
    
    g_grid.init();
    
    m_pMainView = new Viewport();
    m_pMainView->init();
    
    m_pViewportPanel = new ViewportPanel("Perspective");
    m_pViewportPanel->setViewport(m_pMainView);
    
    m_pScenePanel = new ScenePanel();
    m_pScenePanel->setScene(this);
    
    m_pInspectorPanel = new InspectorPanel();
    m_pAssetsPanel = new AssetsPanel();
}

WorldScene::~WorldScene() = default;



void WorldScene::update(float dt)
{
    m_pViewportPanel->update(dt);
}

void WorldScene::draw()
{
    m_pMainView->bind();
    
    auto camera = m_pMainView->getCamera();
    glm::mat4x4 viewProj = camera->projection * camera->view;

    g_grid.draw();
    g_shadedCube.draw();
    
    RenderDevice::commit(camera);
    
    m_pMainView->unbind();
}
