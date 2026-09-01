//
//  SourceMapScene.cpp
//  TryOpenGL
//
//  Created by Fedor Artemenkov on 12.05.25.
//

#include "World.h"

#include "Application.h"
#include "UIManager.h"
#include "InspectorPanel.h"

#include "Camera.h"

#include "Cube.h"
#include "Mesh.h"
#include "Grid.h"
#include "ModelLoader.h"

#include "Entity.h"
#include "AnimatorComponent.h"

#include "Viewport.h"

#include "RenderDevice.h"

#include "Sky.h"

#include "glm/gtc/matrix_transform.hpp"

#include "../vendor/json.hpp"

#include <iostream>
#include <fstream>

static ShadedCube g_shadedCube;
static Mesh g_mesh;
static Grid g_grid;
static Sky g_sky;

//static Entity g_entity("My Entity");

WorldScene::WorldScene()
{
    g_grid.init();
    g_sky.init();
}

WorldScene::~WorldScene() = default;



void WorldScene::update(float dt)
{
//    m_pViewportPanel->update(dt);
    
    for (auto& entity : m_entities)
    {
        if (!entity->m_isActive) continue;
        
        auto animator = entity->getComponent<AnimatorComponent>();
        
        if (animator)
        {
            animator->update(dt);
        }
    }
}

void WorldScene::draw()
{
    RenderDevice::startFrame();
    
//    g_grid.draw();
    
    for (auto& entity : m_entities)
    {
        if (!entity->m_isActive) continue;
        
        auto transformComp = entity->getComponent<TransformComponent>();
        auto meshComp = entity->getComponent<MeshComponent>();
        
        auto model = transformComp ? transformComp->toMat4() : glm::mat4(1.0);
        
        if (meshComp && meshComp->mesh)
        {
            auto animator = entity->getComponent<AnimatorComponent>();
            
            if (!animator || animator->m_clips.empty()) {
                meshComp->mesh->draw(model);
            } else {
                meshComp->mesh->draw(model, animator->getFinalBoneMatrices());
            }
        }
    }
    
    m_pMainView->bind();
    
    auto camera = m_pMainView->getCamera();
    g_sky.draw(camera->view, camera->projection);
    
    RenderDevice::commit(m_pMainView->getCamera());
    
    m_pMainView->unbind();
    
    RenderDevice::endFrame();
}

void WorldScene::selectEntityAtIndex(int index)
{
    if (index < m_entities.size())
    {
        Entity* rawPtr = m_entities[index].get();
        Application::currentInstance->m_pUIManager->setEntity(rawPtr);
    }
}

using json = nlohmann::json;

void to_json(json& entityJson, const Entity& entity)
{
    entityJson["name"] = entity.m_name;
    entityJson["active"] = entity.m_isActive;
    
    json componentsJson = json::object();
    
    for (const auto& comp : entity.getAllComponents())
    {
        json compData;
        comp->save(compData);
        
        // Ключом будет имя компонента, например "TransformComponent"
        componentsJson[comp->getTypeName()] = compData;
    }
    
    entityJson["components"] = componentsJson;
}

void SerializeEntity(json& entityJson, const Entity* entity)
{
    entityJson["name"] = entity->m_name;
    
    json componentsArray = json::array();
    
    for (const auto& comp : entity->getAllComponents())
    {
        json compNode;
        compNode["type"] = comp->getTypeName();
        
        json compData;
        comp->save(compData);
        compNode["data"] = compData;
        
        componentsArray.push_back(compNode);
    }
    
    entityJson["components"] = componentsArray;
}

void WorldScene::save()
{
    json sceneJson;
    
    sceneJson["name"] = "test";
    sceneJson["entities"] = json::array();
    
    for (const auto& entity : m_entities)
    {
        json entityJson;
        SerializeEntity(entityJson, entity.get());
        sceneJson["entities"].push_back(entityJson);
    }
    
    std::ofstream file("scene.json");
    file << sceneJson.dump(4);
}

void DeserializeEntity(const json& entityJson, Entity* entity)
{
    auto componentsArray = entityJson.at("components");
    
    for (const auto& compNode : componentsArray)
    {
        std::string compName = compNode.at("type").get<std::string>();
        const json& compData = compNode.at("data");
        
        Component* newComp = nullptr;
        
        // Фабрика компонентов по их текстовому имени
        if (compName == "TransformComponent")
        {
            newComp = entity->addComponent<TransformComponent>();
        }
        else if (compName == "MeshComponent")
        {
            newComp = entity->addComponent<MeshComponent>();
            
            // После создания компонента, не забудьте накатить на него ресурсы!
            // Берём путь к glb, который прочитается внутри Deserialize, и грузим через ResourceManager
            std::string path = compData.at("source_glb");
            ModelResource model = ModelLoader::LoadModel(path);
            
            auto meshComp = static_cast<MeshComponent*>(newComp);
            meshComp->mesh = model.mesh;
            meshComp->materials = model.embeddedMaterials;
        }
        else if (compName == "AnimatorComponent")
        {
            newComp = entity->addComponent<AnimatorComponent>();
            
            std::string path = compData.at("source_glb");
            ModelResource model = ModelLoader::LoadModel(path);
            
            auto animComp = static_cast<AnimatorComponent*>(newComp);
            animComp->setSkeleton(model.skeleton);
            animComp->m_clips = model.animations;
        }
        
        // Загружаем уникальное состояние (время, позицию, скорость и т.д.)
        if (newComp)
        {
            newComp->load(compData);
        }
    }
}

void WorldScene::load()
{
    std::string filename = "scene.json";
    
    // 1. Открываем и читаем JSON-файл
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Не удалось открыть файл сцены: " << filename << std::endl;
        return;
    }
    
    json sceneJson;
    try {
        file >> sceneJson;
    } catch (const json::parse_error& e) {
        std::cerr << "Ошибка парсинга JSON: " << e.what() << std::endl;
        return;
    }
    
    // Проверяем базовую структуру файла
    if (!sceneJson.contains("entities") || !sceneJson["entities"].is_array()) {
        std::cerr << "Неверный формат сцены: отсутствует массив 'entities'" << std::endl;
        return;
    }
    
    // ==========================================
    // ПРОХОД 1: Создание Entity и их компонентов
    // ==========================================
    for (const auto& entityJson : sceneJson["entities"])
    {
        std::string name = entityJson.value("name", "Unnamed_Entity");
        std::cout << "Load " << name << std::endl;

        auto entity = std::make_unique<Entity>(name);
        
        if (entityJson.contains("components"))
        {
            DeserializeEntity(entityJson, entity.get());
        }
        
        m_entities.push_back(std::move(entity));
    }
}
