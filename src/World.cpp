//
//  SourceMapScene.cpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 12.05.25.
//

#include "World.h"

//#include "CameraController.h"
#include "Camera.h"

#include "Cube.h"
#include "Grid.h"

#include "Viewport.h"

#include "ViewportPanel.h"
#include "ScenePanel.h"
#include "InspectorPanel.h"
#include "AssetsPanel.h"

#include <glad/glad.h>

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
    
    glEnable(GL_DEPTH_TEST);
    
    glClearColor(0.0, 0.0, 0.0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    auto camera = m_pMainView->getCamera();
    glm::mat4x4 viewProj = camera->projection * camera->view;

    g_grid.draw(viewProj);
    g_shadedCube.draw(viewProj);
    
    m_pMainView->unbind();
}
