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

#include "../vendor/json.hpp"

#include <iostream>

static ShadedCube g_shadedCube;
static Mesh g_mesh;
static Grid g_grid;

//static Entity g_entity("My Entity");

WorldScene::WorldScene()
{
    g_grid.init();
    

}

WorldScene::~WorldScene() = default;



void WorldScene::update(float dt)
{
//    m_pViewportPanel->update(dt);
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
            auto model = transformComp->toMat4();
            meshComp->mesh.draw(model);
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
//    if (index < m_entities.size())
//    {
//        Entity* rawPtr = m_entities[index].get();
//        m_pInspectorPanel->setEntity(rawPtr);
//    }
}

using json = nlohmann::json;

void to_json(json& j, const Entity& t)
{
    j = json{ {"name", t.m_name} };
}

void WorldScene::save()
{
    // Создаем пустой JSON-массив для объектов
    json entitiesArray = json::array();
    
    // Вручную разыменовываем каждый unique_ptr
    for (const auto& entityPtr : m_entities)
    {
        if (entityPtr) { // Проверяем на nullptr на всякий случай
            // Записываем сам объект (*entityPtr), библиотека вызовет to_json(json&, const Entity&)
            entitiesArray.push_back(*entityPtr);
        }
    }
    
    auto j = json {
        { "scene_name", "test" },
        { "objects", entitiesArray }
    };
    
    std::cout << j.dump(4) << "\n\n";
}
