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

#include "Grid.h"
#include "ModelLoader.h"

#include "Entity.h"
#include "TransformComponent.h"
#include "AnimatorComponent.h"
#include "ScriptComponent.h"
#include "AudioComponent.h"
#include "MeshComponent.h"

#include "Viewport.h"

#include "RenderDevice.h"
#include "AudioSystem.h"
#include "ScriptEngine.h"

#include "Sky.h"

#include "../vendor/json.hpp"

#include <iostream>
#include <fstream>

static Grid g_grid;
static Sky g_sky;

WorldScene::WorldScene()
{
    g_grid.init();
    g_sky.init();
    AudioSystem::Get().Initialize();
    
    if (ScriptEngine::get().init())
    {
        ScriptEngine::get().CompileAllScripts("assets/scripts");
    }
    else
    {
        std::cerr << "Failed to init AngelScript!" << std::endl;
    }
    
    
    g_currentScene = this;
}

WorldScene::~WorldScene() = default;



void WorldScene::update(float dt)
{
    processPendingStarts();
    
    for (auto* component : m_globalUpdatables)
    {
        if (component->owner->m_isActive && component->enabled) {
            component->update(dt);
        }
    }
    
    // 2. СНАЧАЛА зачищаем указатели «смертников» из систем обновлений Сцены
    cleanupDeadComponents();
    
    // 3. И ТОЛЬКО ПОТОМ физически удаляем сущности из памяти а с ними и сами компоненты
    cleanupDeadEntities();
}

void WorldScene::draw()
{
    RenderDevice::startFrame();
    
    if (m_drawGrid)
    {
        g_grid.draw();
    }
    
    for (auto& entity : m_entities)
    {
        if (!entity->m_isActive) continue;
//        if (entity->isPendingKill()) continue;
        
        auto transformComp = entity->getComponent<TransformComponent>();
        auto meshComp = entity->getComponent<MeshComponent>();
        
        auto model = transformComp ? transformComp->toMat4() : glm::mat4(1.0);
        
        if (meshComp && meshComp->mesh)
        {
            auto animator = entity->getComponent<AnimatorComponent>();
            
            if (!animator || animator->m_clips.empty())
            {
                meshComp->mesh->draw(model);
            }
            else
            {
                meshComp->mesh->draw(model, animator->getFinalBoneMatrices());
            }
        }
    }
    
    // FIXME: установка вьюпорта быть в RenderDevice
    m_pMainView->bind();
    
    if (m_drawSkybox)
    {
        auto camera = m_pMainView->getCamera();
        g_sky.draw(camera->view, camera->projection);
    }
    
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
    
    for (const auto& comp : entity.getComponents())
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
    
    for (const auto& comp : entity->getComponents())
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

static void DeserializeEntity(const json& entityJson, Entity* entity)
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
        else if (compName == "AudioComponent")
        {
            newComp = entity->addComponent<AudioComponent>();
            
            std::string path = compData.at("source");
            
            auto audioComp = static_cast<AudioComponent*>(newComp);
            audioComp->init(path);
        }
        else if (compName == "ScriptComponent")
        {
            newComp = entity->addComponent<ScriptComponent>();
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

Entity *WorldScene::findEntityByName(const std::string &name)
{
    auto it = std::find_if(m_entities.begin(), m_entities.end(),
                           [&name](const std::unique_ptr<Entity>& entity) {
        return entity && entity->m_name == name;
    });
    
    return (it != m_entities.end()) ? it->get() : nullptr;
}

void WorldScene::loadMap(const std::string &filename)
{
    
}

void WorldScene::registerComponent(Component *component)
{
    component->awake(); // Awake вызывается СРАЗУ при создании
    
    if (component->HasFlag(Component::RequiresUpdate)) {
        m_globalUpdatables.push_back(component);
    }
    // Точно так же для физики:
    if (component->HasFlag(Component::RequiresFixedUpdate)) {
        m_globalFixedUpdatables.push_back(component);
    }
    
    m_globalWaitingForStart.push_back(component);
}

void WorldScene::unregisterComponent(Component *component)
{
    // Убираем из очереди Start, если он еще не успел вызваться
    m_globalWaitingForStart.erase(std::remove(m_globalWaitingForStart.begin(), m_globalWaitingForStart.end(), component),
                                  m_globalWaitingForStart.end()
                                  );
    
    m_globalUpdatables.erase(std::remove(m_globalUpdatables.begin(), m_globalUpdatables.end(), component),
                             m_globalUpdatables.end()
                             );
    
    m_globalFixedUpdatables.erase(std::remove(m_globalFixedUpdatables.begin(), m_globalFixedUpdatables.end(), component),
                                  m_globalFixedUpdatables.end()
                                  );
}

void WorldScene::processPendingStarts()
{
    if (m_globalWaitingForStart.empty()) return;
    
    auto processingList = std::move(m_globalWaitingForStart);
    m_globalWaitingForStart.clear();
    
    for (auto* component : processingList)
    {
        if (component->owner->m_isActive && component->enabled)
        {
            component->start();
        }
        else
        {
            m_globalWaitingForStart.push_back(component);
        }
    }
}

void WorldScene::cleanupDeadComponents()
{
    if (m_componentsToDestroy.empty()) return;
    
    for (auto comp : m_componentsToDestroy)
    {
        unregisterComponent(comp);
        comp->owner->freeComponentMemory(comp);
    }
    
    // Очищаем буфер смертников для следующего кадра
    m_componentsToDestroy.clear();
}

void WorldScene::cleanupDeadEntities()
{
    if (m_entitiesToDestroy.empty()) return;
    
    // Делаем копию очереди, так как в процессе удаления родителя
    // могут каскадно вызваться Destroy() для детей, что изменит m_entitiesToDestroy
    auto processingList = std::move(m_entitiesToDestroy);
    m_entitiesToDestroy.clear();
    
    for (Entity* entity : processingList)
    {
        
//        // 1. Обработка иерархии Unreal-style (остается прежней)
//        TransformComponent* transform = entity->getTransform();
//        if (transform)
//        {
//            // Вариант А: Уничтожаем детей вместе с родителем (Каскадное удаление)
//            // Делаем копию вектора детей, так как их Destroy() изменит оригинальный вектор
//            auto children = transform->getChildren();
//            
//            for (TransformComponent* childTransform : children)
//            {
//                childTransform->entity->destroy(); // Вызываем Destroy у дочерней Сущности
//            }
//            
//            // Вариант Б: Если вы НЕ хотите удалять детей, а хотите просто "отвязать" их в мир:
//            // transform->detachAllChildren();
//            
//            // Отвязываем сам удаляемый объект от его родителя
//            transform->setParent(nullptr);
//        }
        
        // 2. Важный момент: Мы НЕ трогаем списки обновлений здесь!
        // Потому что на этом же кадре (чуть раньше или чуть позже) сработает метод
        // CleanupDeadComponents(), который аккуратно вычистит все компоненты
        // этой сущности из m_globalUpdatables, так как они были туда добавлены в Entity::Destroy().
        
        // 3. Физически удаляем только контейнер сущности
        m_entities.erase(std::remove_if(m_entities.begin(), m_entities.end(),
                                        [entity](const std::unique_ptr<Entity>& e) { return e.get() == entity; }),
                         m_entities.end()
                         );
    }
}

