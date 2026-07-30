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

#include "Entity.h"

#include "Viewport.h"

#include "ViewportPanel.h"
#include "ScenePanel.h"
#include "InspectorPanel.h"
#include "AssetsPanel.h"

#include "RenderDevice.h"

#include "glm/gtc/matrix_transform.hpp"

static ShadedCube g_shadedCube;
static Mesh g_mesh;
static Grid g_grid;

//static Entity g_entity("My Entity");

WorldScene::WorldScene()
{
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
    
//    m_pInspectorPanel->setEntity(m_entities[0].get());
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
    
    for (auto& entity : m_entities)
    {
        auto transformComp = entity->getComponent<TransformComponent>();
        auto meshComp = entity->getComponent<MeshComponent>();
        
        if (transformComp && meshComp)
        {
            glm::mat4 model(1.0f);
            model = glm::translate(model, transformComp->position);
            model = glm::scale(model, transformComp->scale);
            
            meshComp->mesh.draw(model, meshComp->tintColor);
        }
    }
    
    m_pMainView->bind();
    RenderDevice::commit(m_pMainView->getCamera());
    m_pMainView->unbind();
    
//    m_pMainView2->bind();
//    RenderDevice::commit(m_pMainView2->getCamera());
//    m_pMainView2->unbind();
    
    RenderDevice::endFrame();
}

void WorldScene::selectEntityAtIndex(int index)
{
    if (index < m_entities.size())
    {
        Entity* rawPtr = m_entities[index].get();
        m_pInspectorPanel->setEntity(rawPtr);
    }
}
