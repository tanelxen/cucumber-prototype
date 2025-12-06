//
//  SourceMapScene.cpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 12.05.25.
//

#include "World.h"

#include "CameraController.h"
#include "Camera.h"

#include "Cube.h"
#include "Grid.h"

#include <glad/glad.h>

static ShadedCube g_shadedCube;
static Grid g_grid;

WorldScene::WorldScene(Camera *camera) : m_pCamera(camera)
{
    m_pPlayer = std::make_unique<CameraController>(camera);
    m_pPlayer->position = {0, 128, 128};
    
    g_shadedCube.init();
    g_shadedCube.scale = { 64, 64, 64 };
    g_shadedCube.position = { 0, 0, -64 };
    
    g_grid.init();
}

WorldScene::~WorldScene() = default;



void WorldScene::update(float dt)
{
    if (m_pCamera == nullptr) return;
    
    m_pPlayer->update(dt);
}

void WorldScene::draw()
{
    if (m_pCamera == nullptr) return;
    
    glEnable(GL_DEPTH_TEST);
    
    glClearColor(0.1, 0.1, 0.1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glm::mat4x4 viewProj = m_pCamera->projection * m_pCamera->view;

    g_grid.draw(viewProj);
    g_shadedCube.draw(viewProj);
}
