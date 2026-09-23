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
#include "GizmoRender.h"

#include "ModelLoader.h"

#include "Entity.h"
#include "TransformComponent.h"
#include "MovementComponent.h"
#include "AnimatorComponent.h"
#include "ScriptComponent.h"
#include "CameraComponent.h"
#include "AudioComponent.h"
#include "MeshComponent.h"

#include "Viewport.h"

#include "RenderDevice.h"
#include "AudioSystem.h"
#include "ScriptEngine.h"

#include "Sky.h"

#include "q3/Q3BSPAsset.h"
#include "q3/Q3BspMesh.h"
#include "q3/Q3BspCollision.h"

#include "PhysicsCommon.h"

#include "../vendor/json.hpp"

#include <iostream>
#include <fstream>

static Grid g_grid;
static Sky g_sky;

static Q3BspMesh g_mesh;
static Q3BspCollision g_collision;

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
    
    loadBSP("assets/wolf/maps/escape2.bsp");
    
    g_currentScene = this;
}

WorldScene::~WorldScene() = default;



void WorldScene::update(float dt)
{
    processPendingStarts();
    
    if (m_isSimulationMode)
    {
        for (auto* component : m_globalUpdatables)
        {
            if (component->owner->m_isActive && component->enabled) {
                component->update(dt);
            }
        }
    }
    
    for (auto* component : m_globalUpdatables)
    {
        component->gizmo();
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
    
    g_mesh.draw();
    
    for (auto& entity : m_entities)
    {
        if (!entity->m_isActive) continue;
//        if (entity->isPendingKill()) continue;
        
        auto meshComp = entity->getComponent<MeshComponent>();
        
        if (meshComp && meshComp->mesh)
        {
            auto worldMatrix = entity->getTransform().getWorldMatrix();
            
            auto animator = entity->getComponent<AnimatorComponent>();
            
            if (!animator || animator->m_clips.empty())
            {
                meshComp->mesh->draw(worldMatrix);
            }
            else
            {
                meshComp->mesh->draw(worldMatrix, animator->getFinalBoneMatrices());
            }
        }
    }
    
    // FIXME: установка вьюпорта должна быть в RenderDevice
    m_pAttachedViewport->bind();
    
    if (m_drawSkybox)
    {
        auto camera = m_pAttachedViewport->getCamera();
        g_sky.draw(camera->view, camera->projection);
    }
    
    {
        auto camera = m_pAttachedViewport->getCamera();
        GizmoRender::Get().Flush(camera->view, camera->projection);
    }
    
    RenderDevice::commit(m_pAttachedViewport->getCamera());
    

    
    m_pAttachedViewport->unbind();
    
    RenderDevice::endFrame();
    GizmoRender::Get().Clear();
}

void WorldScene::selectEntityAtIndex(int index)
{
    if (index < m_entities.size())
    {
        Entity* rawPtr = m_entities[index].get();
        Application::currentInstance->m_pUIManager->setEntity(rawPtr);
    }
}

void SerializeEntity(nlohmann::json& entityJson, const Entity* entity)
{
    entityJson["name"] = entity->m_name;
    entityJson["uid"] = entity->uid;
    entityJson["parent"] = entity->parent ? entity->parent->uid : UID::Invalid();
    
    nlohmann::json componentsArray = nlohmann::json::array();
    
    {
        auto comp = entity->getTransformPtr();
        
        nlohmann::json compNode;
        compNode["type"] = comp->getTypeName();
        
        nlohmann::json compData;
        comp->save(compData);
        compNode["data"] = compData;
        
        componentsArray.push_back(compNode);
    }
    
    for (const auto& comp : entity->getComponents())
    {
        nlohmann::json compNode;
        compNode["type"] = comp->getTypeName();
        
        nlohmann::json compData;
        comp->save(compData);
        compNode["data"] = compData;
        
        componentsArray.push_back(compNode);
    }
    
    entityJson["components"] = componentsArray;
}

void WorldScene::save(const std::string& filename)
{
    nlohmann::json sceneJson;
    
    sceneJson["name"] = "test";
    sceneJson["entities"] = nlohmann::json::array();
    
    for (const auto& entity : m_entities)
    {
        nlohmann::json entityJson;
        SerializeEntity(entityJson, entity.get());
        sceneJson["entities"].push_back(entityJson);
    }
    
    std::ofstream file(filename);
    file << sceneJson.dump(4);
}

static void DeserializeEntity(const nlohmann::json& entityJson, Entity* entity)
{
    auto componentsArray = entityJson.at("components");
    
    for (const auto& compNode : componentsArray)
    {
        std::string compName = compNode.at("type").get<std::string>();
        const nlohmann::json& compData = compNode.at("data");
        
        Component* newComp = nullptr;
        
        // Фабрика компонентов по их текстовому имени
        if (compName == "TransformComponent")
        {
//            newComp = entity->addComponent<TransformComponent>();
            newComp = &entity->getTransform();
        }
        else if (compName == "MeshComponent")
        {
            newComp = entity->addComponent<MeshComponent>();
            
            // После создания компонента, не забудьте накатить на него ресурсы!
            // Берём путь к glb, который прочитается внутри Deserialize, и грузим через ResourceManager
            std::string path = compData.at("source_glb");
            ModelResource model = ModelLoader::LoadModel("assets/" + path);
            
            auto meshComp = static_cast<MeshComponent*>(newComp);
            meshComp->mesh = model.mesh;
            meshComp->materials = model.embeddedMaterials;
        }
        else if (compName == "AnimatorComponent")
        {
            newComp = entity->addComponent<AnimatorComponent>();
            
            std::string path = compData.at("source_glb");
            ModelResource model = ModelLoader::LoadModel("assets/" + path);
            
            auto animComp = static_cast<AnimatorComponent*>(newComp);
            animComp->setSkeleton(model.skeleton);
            animComp->m_clips = model.animations;
            animComp->path = path; // FIXME: HACK
        }
        else if (compName == "AudioComponent")
        {
            newComp = entity->addComponent<AudioComponent>();
            
            std::string path = compData.at("source");
            
            auto audioComp = static_cast<AudioComponent*>(newComp);
            audioComp->init("assets/" + path);
        }
        else if (compName == "ScriptComponent")
        {
            newComp = entity->addComponent<ScriptComponent>();
        }
        else if (compName == "CameraComponent")
        {
            newComp = entity->addComponent<CameraComponent>();
        }
        else if (compName == "MovementComponent")
        {
            newComp = entity->addComponent<MovementComponent>();
        }
        
        // Загружаем уникальное состояние (время, позицию, скорость и т.д.)
        if (newComp)
        {
            newComp->load(compData);
        }
    }
}

void WorldScene::load(const std::string& filename)
{
    // 1. Открываем и читаем JSON-файл
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Не удалось открыть файл сцены: " << filename << std::endl;
        return;
    }
    
    nlohmann::json sceneJson;
    try {
        file >> sceneJson;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "Ошибка парсинга JSON: " << e.what() << std::endl;
        return;
    }
    
    // Проверяем базовую структуру файла
    if (!sceneJson.contains("entities") || !sceneJson["entities"].is_array()) {
        std::cerr << "Неверный формат сцены: отсутствует массив 'entities'" << std::endl;
        return;
    }
    
    std::unordered_map<UID, Entity*> entityMap;
    
    // ==========================================
    // ПРОХОД 1: Создание Entity и их компонентов
    // ==========================================
    for (const auto& entityJson : sceneJson["entities"])
    {
        std::string name = entityJson.value("name", "Unnamed_Entity");
        std::cout << "Load " << name << std::endl;

        auto entity = std::make_unique<Entity>(name);
        entity->uid = entityJson.at("uid").get<UID>();
        
        if (entityJson.contains("components"))
        {
            DeserializeEntity(entityJson, entity.get());
        }
        
        entityMap[entity->uid] = entity.get();
        m_entities.push_back(std::move(entity));
    }
    
    // ВТОРОЙ ПРОХОД: Восстанавливаем связи иерархии Parent-Child по ID
    for (const auto& j : sceneJson["entities"])
    {
        UID entityId = j.at("uid").get<UID>();
        UID parentId = j.at("parent").get<UID>();
        
        if (parentId != 0)
        {
            Entity* child = entityMap[entityId];
            Entity* parent = entityMap[parentId];
            
            if (child && parent) {
                child->setParent(parent, false); // Связываем их!
            }
        }
    }
}

Entity* WorldScene::findEntityByName(const std::string &name)
{
    auto it = std::find_if(m_entities.begin(), m_entities.end(),
                           [&name](const std::unique_ptr<Entity>& entity) {
        return entity && entity->m_name == name;
    });
    
    return (it != m_entities.end()) ? it->get() : nullptr;
}

void WorldScene::loadBSP(const std::string &filename)
{
    Q3BSPAsset bsp;
    
    if (!bsp.initFromFile(filename.c_str())) {
        return;
    }
    
    g_mesh.initFromBsp(&bsp);
    g_collision.initFromBsp(&bsp);
}

void WorldScene::queueComponentDestruction(Component* component) {
    m_componentsToDestroy.push_back(component);
}

void WorldScene::queueEntityDestruction(Entity* entity)
{
    // Проверяем, нет ли его уже в очереди
    if (std::find(m_entitiesToDestroy.begin(), m_entitiesToDestroy.end(), entity) == m_entitiesToDestroy.end()) {
        m_entitiesToDestroy.push_back(entity);
    }
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
        m_entities.erase(std::remove_if(m_entities.begin(), m_entities.end(),
                                        [entity](const std::unique_ptr<Entity>& e) { return e.get() == entity; }),
                         m_entities.end()
                         );
    }
}

void WorldScene::startSimulation(Viewport* viewport)
{
    if (!viewport) return;
    
    m_pAttachedViewport = viewport;
    m_isSimulationMode = true;
    
    // 1. Ищем игровую камеру на сцене
    Camera* gameCamera = nullptr;
    
    for (auto& entity : m_entities) {
        if (!entity->m_isActive) continue;
        
        if (auto cameraComp = entity->getComponent<CameraComponent>()) {
            gameCamera = &cameraComp->m_camera;
            break;
        }
    }
    
    // 2. Если нашли игровую камеру — подменяем её во вьюпорте
    if (gameCamera) {
        m_pAttachedViewport->setCamera(gameCamera);
        // Опционально: отключаем контроллер свободной камеры вьюпорта,
        // чтобы мышь/WASD редактора не двигали игровую камеру
//        m_pAttachedViewport->getCameraController()->setActive(false);
    }
    
    // 3. Запускаем логику симуляции (например, вызов функций Start у скриптов)
    // onSimulationStart();
}

void WorldScene::stopSimulation()
{
    m_isSimulationMode = false;
}

void WorldScene::trace(HitResult &result, const glm::vec3 &start, const glm::vec3 &end, const glm::vec3 &mins, const glm::vec3 &maxs) const
{
    g_collision.trace(result, start, end, mins, maxs);
}
