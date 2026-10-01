//
//  WorldScene.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 12.05.25.
//

#include "World.h"

#include "Application.h"
#include "UIManager.h"

#include "GizmoRender.h"
#include "Grid.h"

#include "ModelLoader.h"

#include "Entity.h"
#include "TransformComponent.h"
#include "CollisionComponent.h"
#include "MovementComponent.h"
#include "AnimatorComponent.h"
#include "ScriptComponent.h"
#include "CameraComponent.h"
#include "AudioComponent.h"
#include "MeshComponent.h"

#include "RenderDevice.h"

#include "Sky.h"

#include "q3/Q3BSPAsset.h"
#include "q3/Q3BspMesh.h"
#include "q3/Q3BspCollision.h"

#include "PhysicsCommon.h"

#include "SpriteRenderer.h"
#include "DecalManager.h"
#include "GameUI.h"

#include "../vendor/json.hpp"

#include <iostream>
#include <fstream>

// Вообще отрисовку сетки надо перенести в GizmoRenderer, но пока что тут
static Grid g_grid;

static Sky g_sky;

// Скорее всего переедет в отдельную Entity
static Q3BspMesh g_mesh;
static Q3BspCollision g_collision;

WorldScene::WorldScene()
{
    g_grid.init();
    g_sky.init();
    
    // Временный хак пока не решу куда вставить bsp-меш и коллизию
    loadBSP("assets/wolf/maps/escape2.bsp");
    
    g_currentScene = this;
}

WorldScene::~WorldScene() = default;



void WorldScene::update(float dt)
{
    if (m_isSimulationMode)
    {
        for (auto* component : m_globalUpdatables)
        {
            if (component->owner->m_isActive && component->enabled) {
                component->update(dt);
            }
        }
    }
    
    for (auto* component : m_allComponents)
    {
        component->gizmo();
    }
    
    cleanupDeadComponents();
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
    
    // Установка вьюпорта должна быть в RenderDevice, наверно
    m_pAttachedViewport->bind();
    {
        if (m_drawSkybox)
        {
            auto camera = m_pAttachedViewport->getCamera();
            g_sky.draw(camera->view, camera->projection);
        }
        
        if (!m_isSimulationMode)
        {
            auto camera = m_pAttachedViewport->getCamera();
            GizmoRender::Get().Flush(camera->view, camera->projection);
        }
        
        RenderDevice::commit(m_pAttachedViewport->getCamera());
        
        {
            auto camera = m_pAttachedViewport->getCamera();
            SpriteRenderer::Flush(camera->view, camera->projection);
            DecalManager::Flush(camera->view, camera->projection);
        }
        
        {
            int width = m_pAttachedViewport->getWidth();
            int height = m_pAttachedViewport->getHeight();
            GameUI::Resize(width, height);
            GameUI::Flush();
        }
    }
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

static void SerializeEntity(nlohmann::json& entityJson, const Entity* entity);
static void DeserializeEntity(const nlohmann::json& entityJson, Entity* entity);

void WorldScene::save(const std::string& filename)
{
    nlohmann::json sceneJson;
    
    sceneJson["name"] = "test";
    sceneJson["entities"] = nlohmann::json::array();
    
    for (const auto& entity : m_entities)
    {
        if (entity->isPrefabChild()) continue;
        
        nlohmann::json entityJson;
        SerializeEntity(entityJson, entity.get());
        sceneJson["entities"].push_back(entityJson);
    }
    
    std::ofstream file(filename);
    file << sceneJson.dump(4);
}

void WorldScene::load(const std::string& filename)
{
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
        if (entityJson.contains("prefab_path"))
        {
            std::string prefabPath = entityJson["prefab_path"];
            UID savedUid = entityJson["uid"].get<UID>();
            
            // 1. Спавним префаб (он создается с дефолтными транформациями из файла префаба)
            Entity* prefabRoot = instantiatePrefab(prefabPath, nullptr, savedUid);
            
            // 2. Если спавн прошел успешно и в сцене сохранены уникальные трансформации
            if (prefabRoot && entityJson.contains("transform_override"))
            {
                // Перезаписываем дефолтный трансформ сценовыми данными
                prefabRoot->getTransformPtr()->load(entityJson["transform_override"]);
            }
        }
        else
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
                child->setParent(parent, false);
            }
        }
    }
    
    // ВСЕ сущности созданы, ВСЕ parent-child связи восстановлены.
    // Теперь безопасно вызываем awake() для абсолютно всех компонентов на сцене:
    for (auto* component : m_allComponents)
    {
        component->awake();
    }
}

void WorldScene::saveAsPrefab(Entity* entity, const std::string &filename)
{
    nlohmann::json prefabJson;
    
    prefabJson["name"] = entity->m_name;
    prefabJson["root"] = entity->uid;
    prefabJson["entities"] = nlohmann::json::array();
    
    // Лямбда-функция захватывает prefabJson по ссылке [&prefabJson]
    auto serializeRecursively = [&prefabJson](auto& self, Entity* currentEntity) -> void {
        if (!currentEntity) return; // Защита от нулевого указателя
        
        nlohmann::json entityJson;
        SerializeEntity(entityJson, currentEntity);
        prefabJson["entities"].push_back(entityJson);
        
        for (const auto& child : currentEntity->children)
        {
            self(self, child); // Рекурсивный вызов: передаем себя и ребенка
        }
    };
    
    // Запускаем рекурсию для корневого элемента
    serializeRecursively(serializeRecursively, entity);
    
    std::ofstream file(filename);
    file << prefabJson.dump(4);
}

void WorldScene::loadPrefab(Entity* entity, const std::string &filename)
{
    entity = instantiatePrefab(filename, nullptr);
}

static void RemapJsonUIDs(nlohmann::json& j, const std::unordered_map<std::string, std::string>& txtRemap);

Entity* WorldScene::instantiatePrefab(const std::string& filename, Entity* customParent, UID forcedRootUid)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Не удалось открыть файл префаба: " << filename << std::endl;
        return nullptr;
    }
    
    nlohmann::json prefabJson;
    try {
        file >> prefabJson;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "Ошибка парсинга JSON префаба: " << e.what() << std::endl;
        return nullptr;
    }
    
    nlohmann::json entitiesArray;
    if (prefabJson.is_array()) {
        entitiesArray = prefabJson;
    } else if (prefabJson.contains("entities")) {
        entitiesArray = prefabJson["entities"];
    } else {
        entitiesArray = nlohmann::json::array({ prefabJson });
    }
    
    // Таблица соответствий текстовых UID: [Старая строка] -> [Новая строка]
    std::unordered_map<std::string, std::string> stringUidRemap;
    std::unordered_map<UID, UID> oldToNew;
    std::unordered_map<UID, UID> newToOld;
    
    
    // Определяем, кто в файле префаба является корнем (у кого нет родителя ИЛИ родитель внешний)
    // Так как у нас ограничение на одну корневую сущность, мы легко ее найдем
    UID rootOldUid = UID::Invalid();
    for (const auto& entityJson : entitiesArray) {
        if (!entityJson.contains("parent") || entityJson["parent"].is_null()) {
            rootOldUid = entityJson.at("uid").get<UID>(); // это старый UID корня до ремапа
            break;
        }
    }
    
    // Если по JSON не нашли, берем просто первый элемент массива за корень
    if (!rootOldUid.isValid() && !entitiesArray.empty()) {
        rootOldUid = entitiesArray[0].at("uid").get<UID>();
    }
    
    // ШАГ 1: Генерируем новые ID и составляем карту замен
    for (const auto& entityJson : entitiesArray)
    {
        UID oldUid = entityJson.at("uid").get<UID>();
        UID newUid = (oldUid == rootOldUid && forcedRootUid.isValid()) ? forcedRootUid : UID();
        
        stringUidRemap[oldUid.toText()] = newUid.toText();
        
        oldToNew[oldUid] = newUid;
        newToOld[newUid] = oldUid;
    }
    
    // ШАГ 2: АВТОМАТИЧЕСКИЙ РЕМАППИНГ ВСЕГО JSON
    // Проходим по всему массиву. Все упоминания старых UID внутри любых компонентов
    // (будь то ссылки на другие Entity или связи между компонентами) заменяются на новые.
    RemapJsonUIDs(entitiesArray, stringUidRemap);
    
    // ШАГ 3: Десериализация (используем ваш стандартный DeserializeEntity!)
    std::vector<Entity*> instantiatedEntities;
    std::unordered_map<UID, Entity*> localIdToEntityMap;
    
    for (const auto& entityJson : entitiesArray)
    {
        std::string name = entityJson.value("name", "Prefab_Instance");
        UID newUid = entityJson.at("uid").get<UID>(); // Здесь уже НОВЫЙ ремапленный UID
        
        auto entity = std::make_unique<Entity>(name);
        entity->uid = newUid;
        
        // ПОМЕЧАЕМ СУЩНОСТЬ
        entity->prefabAssetPath = filename;
        // Проверяем, совпадает ли этот объект с вычисленным корнем префаба
        // stringUidRemap[rootOldUid.toText()] вернет новый перемапленный UID корня
        if (newUid.toText() == stringUidRemap[rootOldUid.toText()]) {
            entity->isPrefabRoot = true;
        }
        
        if (entityJson.contains("components"))
        {
            DeserializeEntity(entityJson, entity.get());
        }
        
        Entity* rawPtr = entity.get();
        localIdToEntityMap[newUid] = rawPtr;
        instantiatedEntities.push_back(rawPtr);
        
        m_entities.push_back(std::move(entity));
    }
    
    // ШАГ 4: Восстановление иерархии Parent-Child
    Entity* rootEntity = nullptr;
    for (const auto& entityJson : entitiesArray)
    {
        UID newUid = entityJson.at("uid").get<UID>();
        UID newParentUid = entityJson.value("parent", UID::Invalid()); // Здесь parent уже тоже ремапленный!
        
        Entity* child = localIdToEntityMap[newUid];
        
        if (newParentUid.isValid())
        {
            Entity* parent = localIdToEntityMap[newParentUid];
            if (child && parent) {
                child->setParent(parent, false);
            }
        }
    }
    
    if (!rootEntity)
    {
        rootEntity = instantiatedEntities[0];
    }
    
    if (customParent && rootEntity)
    {
        rootEntity->setParent(customParent, false);
    }
    
    for (Entity* entity : instantiatedEntities)
    {
        entity->originalPrefabUid = newToOld[entity->uid];
        entity->awake();
    }
    
    return rootEntity;
}


Entity* WorldScene::findEntityByName(const std::string &name)
{
    auto it = std::find_if(m_entities.begin(), m_entities.end(),
                           [&name](const std::unique_ptr<Entity>& entity) {
        return entity && entity->m_name == name;
    });
    
    return (it != m_entities.end()) ? it->get() : nullptr;
}

Entity* WorldScene::findEntityByUID(const UID& uid)
{
    auto it = std::find_if(m_entities.begin(), m_entities.end(),
                           [&uid](const std::unique_ptr<Entity>& entity) {
        return entity && entity->uid == uid;
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
    if (component->HasFlag(Component::RequiresUpdate)) {
        m_globalUpdatables.push_back(component);
    }
    
    // Регистрируем коллайдеры в выделенный список
    if (auto* collider = dynamic_cast<CollisionComponent*>(component)) {
        m_globalColliders.push_back(collider);
    }
    
    m_allComponents.push_back(component);
}

void WorldScene::unregisterComponent(Component *component)
{
    m_allComponents.erase(std::remove(m_allComponents.begin(), m_allComponents.end(), component),
                                  m_allComponents.end()
                                  );
    
    m_globalUpdatables.erase(std::remove(m_globalUpdatables.begin(), m_globalUpdatables.end(), component),
                             m_globalUpdatables.end()
                             );
    
    // Удаляем из списка коллайдеров
    // А нужен ли тут dynamic_cast???
    if (auto* collider = dynamic_cast<CollisionComponent*>(component)) {
        m_globalColliders.erase(std::remove(m_globalColliders.begin(), m_globalColliders.end(), collider), m_globalColliders.end());
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
    }
    
    // 3. Запускаем логику симуляции (например, вызов функций Start у скриптов)
    for (auto* component : m_allComponents)
    {
        component->start();
    }
}

void WorldScene::stopSimulation()
{
    m_isSimulationMode = false;
}

// Временно вручную - потом через Jolt
void WorldScene::trace(HitResult &result,
                       const glm::vec3 &q_start,
                       const glm::vec3 &q_end,
                       const glm::vec3 &q_mins,
                       const glm::vec3 &q_maxs,
                       Entity* ignore,
                       uint32_t mask) const
{
    // Инициализируем новые поля дефолтными значениями
    result.hitEntity = nullptr;
    result.hitCollider = nullptr;
    
    // 1. Сначала считаем коллизию со статичной BSP геометрией Quake 3
    g_collision.trace(result, q_start, q_end, q_mins, q_maxs);
    
    const glm::vec3 gl_start = Quake2OpenGL(q_start);
    const glm::vec3 gl_end = Quake2OpenGL(q_end);
    const glm::vec3 gl_mins = Quake2OpenGL(q_mins);
    const glm::vec3 gl_maxs = Quake2OpenGL(q_maxs);
    
    float closestFraction = result.fraction;
    glm::vec3 entityNormal = Quake2OpenGL(result.normal);
    CollisionComponent* closestCollider = nullptr;
    bool hitDynamicEntity = false;
    
    // 2. Итерируемся по динамическим сущностям сцены
    for (const auto& collider : m_globalColliders)
    {
        // Пропускаем неактивные сущности или те, что ожидают удаления
        if (!collider || !collider->enabled || collider->isPendingKill) continue;
        if (!collider->owner->m_isActive || collider->owner->isPendingKill()) continue;
        
        // ИГНОРИРОВАНИЕ СЕБЯ: Пропускаем, если это наш собственный коллайдер
        if (collider->owner == ignore) continue;
        
        // ПРОВЕРКА МАСКИ: Объект должен подходить под маску трассировки
        if ((collider->layer & mask) == 0) continue;
        
        float entityFrac = 1.0f;
        glm::vec3 localNormal = glm::vec3(0.0f);
        
        // Тестируем отрезок от start до текущего найденного максимума (currentEnd)
        if (collider->sweepTest(gl_start, gl_end, gl_mins, gl_maxs, entityFrac, localNormal))
        {
            // Это зона-триггер
            if (collider->isTrigger)
            {
                // Триггеры НЕ прерывают полет луча/ракеты, они просто регистрируют пересечение.
                // Вызываем коллбэк на триггере (например, OnTriggerEnter), передавая туда того, кто в него вошел.
                if (ignore)
                {
                    collider->CallTriggerEnter(ignore);
                }

                continue; // Летим дальше, не уменьшая closestFraction!
            }
            
            // Берем только если этот объект БЛИЖЕ, чем всё, что мы нашли ранее (включая стены BSP)
            if (entityFrac < closestFraction)
            {
                closestFraction = entityFrac;
                closestCollider = collider;
                entityNormal = localNormal;
                hitDynamicEntity = true;
            }
        }
    }
    
    // 3. Если мы перезаписали точку коллизии динамическим объектом, обновляем результирующий HitResult
    if (hitDynamicEntity)
    {
        result.fraction = closestFraction;
        result.endpos = OpenGL2Quake(gl_start + closestFraction * (gl_end - gl_start));
        result.normal = OpenGL2Quake(entityNormal);
        result.hitEntity = closestCollider->owner;
        result.hitCollider = closestCollider;
        
        // Обнуляем Quake-специфичные флаги текстур брашей, так как попали в динамический объект
        result.textureId = -1;
        result.surfaceFlags = 0;
        result.startsolid = (closestFraction == 0.0f);
        result.allsolid = false; // Для динамических объектов рассчитывается индивидуально
    }
}

// FIXME: надо вынести в фабрику компонентов
static void SerializeEntity(nlohmann::json& entityJson, const Entity* entity)
{
    entityJson["name"] = entity->m_name;
    entityJson["uid"] = entity->uid;
    entityJson["parent"] = entity->parent ? entity->parent->uid : UID::Invalid();
    
    // Если это корень префаба, сохраняем только ссылку на файл
    if (entity->isPrefabRoot)
    {
        entityJson["prefab_path"] = entity->prefabAssetPath;
        
        // ПРИНУДИТЕЛЬНО сохраняем трансформацию именно этого экземпляра на сцене
        auto transformJson = nlohmann::json::object();
        entity->getTransform().save(transformJson);
        
        entityJson["transform_override"] = transformJson;
        return; // Больше ничего для префаба в файл сцены не пишем (остальное в .prefab)
    }
    
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

// Надо будет вынести в фабрику компонентов
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
            newComp = &entity->getTransform();
        }
        else if (compName == "MeshComponent")
        {
            newComp = entity->addComponent<MeshComponent>();
            
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
            animComp->path = path; // HACK
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
        else if (compName == "CollisionComponent")
        {
            newComp = entity->addComponent<CollisionComponent>();
        }
        
        // Загружаем уникальное состояние
        if (newComp)
        {
            newComp->load(compData);
        }
    }
}

#include <algorithm>

static void RemapJsonUIDs(nlohmann::json& j, const std::unordered_map<std::string, std::string>& txtRemap)
{
    if (j.is_string())
    {
        std::string val = j.get<std::string>();
        if (val.starts_with("uid://") && val != "uid://<invalid>") // Проверяем, что строка является UID
        {
            auto it = txtRemap.find(val);
            if (it != txtRemap.end())
            {
                j = it->second; // Заменяем старый UID на новый прямо в JSON
            }
        }
    }
    else if (j.is_object())
    {
        for (auto& [key, value] : j.items())
        {
            RemapJsonUIDs(value, txtRemap);
        }
    }
    else if (j.is_array())
    {
        for (auto& element : j)
        {
            RemapJsonUIDs(element, txtRemap);
        }
    }
}
