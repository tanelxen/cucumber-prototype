//
//  SourceMapScene.cpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 12.05.25.
//

#include "World.h"

#include "Camera.h"

#include "Cube.h"
#include "Mesh.h"
#include "Grid.h"
#include "ModelLoader.h"

#include "Viewport.h"

#include "ViewportPanel.h"
#include "ScenePanel.h"
#include "InspectorPanel.h"
#include "AssetsPanel.h"

#include "RenderDevice.h"

static ShadedCube g_shadedCube;
static Mesh g_mesh;
static Grid g_grid;

WorldScene::WorldScene()
{
    g_shadedCube.init();
    g_shadedCube.scale = { 64, 64, 64 };
    g_shadedCube.position = { 0, 0, -64 };
    g_shadedCube.color = { 0.5, 0.0, 0.5, 1.0 };
    g_shadedCube.m_name = "Shaded Cube";
    
    ModelLoader::LoadOBJ("assets/meshes/cube-tex.obj", g_mesh);
    g_mesh.scale = { 64, 64, 64 };
    g_mesh.position = { 0, 0, -64 };
    g_mesh.color = { 0.5, 0.0, 0.5, 1.0 };
    g_mesh.m_name = "cube.obj";
    
    g_grid.init();
    
    m_pMainView = new Viewport();
    m_pMainView->init();
    
    m_pViewportPanel = new ViewportPanel("Perspective");
    m_pViewportPanel->setViewport(m_pMainView);
    
    
//    m_pMainView2 = new Viewport();
//    m_pMainView2->init();
    
//    m_pViewportPanel2 = new ViewportPanel("Perspective 2");
//    m_pViewportPanel2->setViewport(m_pMainView2);
    

    
    m_pScenePanel = new ScenePanel();
    m_pScenePanel->setScene(this);
    
    m_pInspectorPanel = new InspectorPanel();
    m_pAssetsPanel = new AssetsPanel();
    
    m_pInspectorPanel->setEntity(&g_mesh);
}

WorldScene::~WorldScene() = default;



void WorldScene::update(float dt)
{
    m_pViewportPanel->update(dt);
//    m_pViewportPanel2->update(dt);
}

void WorldScene::draw()
{
    RenderDevice::startFrame();
    
    g_grid.draw();
    g_mesh.draw();
//    g_shadedCube.draw();
    
    m_pMainView->bind();
    RenderDevice::commit(m_pMainView->getCamera());
    m_pMainView->unbind();
    
//    m_pMainView2->bind();
//    RenderDevice::commit(m_pMainView2->getCamera());
//    m_pMainView2->unbind();
    
    RenderDevice::endFrame();
}
