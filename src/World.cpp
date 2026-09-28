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
#include "CollisionComponent.h"
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

#include "SpriteRenderer.h"
#include "ParticleSystem.h"
#include "DecalManager.h"
#include "HUD.h"

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
    
    SpriteRenderer::Init();
    ParticleSystem::Init();
    DecalManager::Init();
    HUD::Init();
    
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
//    processPendingStarts();
    
    if (m_isSimulationMode)
    {
        for (auto* component : m_globalUpdatables)
        {
            if (component->owner->m_isActive && component->enabled) {
                component->update(dt);
            }
        }
    }
    
    for (auto* component : m_globalWaitingForStart)
    {
        component->gizmo();
    }
    
    ParticleSystem::Update(dt);
    DecalManager::Update(dt);
    
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
        HUD::Resize(width, height);
        HUD::Flush();
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
        else if (compName == "CollisionComponent")
        {
            newComp = entity->addComponent<CollisionComponent>();
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
    
    for (auto& entity : m_entities)
    {
        auto scripts = entity->getComponents<ScriptComponent>();
        
        for (auto& script : scripts)
        {
            script->loadStateSnapshot();
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
    component->awake(); // Awake вызывается СРАЗУ при создании
    
    if (component->HasFlag(Component::RequiresUpdate)) {
        m_globalUpdatables.push_back(component);
    }
    // Точно так же для физики:
    if (component->HasFlag(Component::RequiresFixedUpdate)) {
        m_globalFixedUpdatables.push_back(component);
    }
    
    // Регистрируем коллайдеры в выделенный список
    if (auto* collider = dynamic_cast<CollisionComponent*>(component)) {
        m_globalColliders.push_back(collider);
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
    
    // Удаляем из списка коллайдеров
    if (auto* collider = dynamic_cast<CollisionComponent*>(component)) {
        m_globalColliders.erase(std::remove(m_globalColliders.begin(), m_globalColliders.end(), collider), m_globalColliders.end());
    }
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
    
    // FIXME: HACK
    for (auto* component : m_globalWaitingForStart)
    {
        component->start();
    }
}

void WorldScene::stopSimulation()
{
    m_isSimulationMode = false;
}


// Хелпер для вызова AngelScript коллбэка
void NotifyScriptCallback(Entity* entityToNotify, const std::string& callbackName, Entity* otherEntity)
{
    if (!entityToNotify || callbackName.empty() || !otherEntity) return;
    
    // Ищем скрипт на объекте, который должен получить уведомление
    auto scriptComp = entityToNotify->getComponent<ScriptComponent>();
    if (!scriptComp) return;
    
    // В AngelScript вызов метода объекта выглядит примерно так через asIScriptContext:
    asIScriptEngine* engine = ScriptEngine::get().GetRawEngine();
    
    // Ищем метод в объекте скрипта (у вас в ScriptComponent скорее всего хранится asIScriptObject)
    asIScriptObject* scriptObject = scriptComp->m_ScriptObject;
    if (!scriptObject) return;
    
    asIScriptFunction* func = scriptObject->GetObjectType()->GetMethodByName(callbackName.c_str());
    if (!func) return; // Метод не объявлен в скрипте (например, у объекта просто нет функции OnTriggerEnter)
    
    asIScriptContext* ctx = engine->CreateContext();
    ctx->Prepare(func);
    ctx->SetObject(scriptObject);
    
    // Передаем @otherEntity в качестве первого аргумента (индекс 0)
    ctx->SetArgAddress(0, otherEntity);
    
    int r = ctx->Execute();
    if (r != asEXECUTION_FINISHED) {
        // Обработка ошибок выполнения скрипта
    }
    ctx->Release();
}

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
        
        /*
        // --- КОЛЛБЭК ДЛЯ РАКЕТЫ ИЛИ СНАРЯДА ---
        // Если тот, кто пускал луч (ignoreCollider), имеет специальный скрипт для взрыва
        if (ignore)
        {
            // Например, если ракета врезалась во врага, вызываем у нее "OnProjectileHit"
            NotifyScriptCallback(ignore, "OnProjectileHit", closestCollider->owner);
        }
         */
    }
//    else if (result.fraction < 1.0f && ignore)
//    {
//        // Ракеты также должны взрываться при ударе о статичную BSP-карту (стены уровня)
//        NotifyScriptCallback(ignore, "OnProjectileHit", nullptr);
//    }
}
