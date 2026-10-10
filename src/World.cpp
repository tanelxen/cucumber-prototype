//
//  WorldScene.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 12.05.25.
//

#include "World.h"
#include "Input.h"

#include "UIManager.h"

#include "Entity.h"

#include "CollisionComponent.h"
#include "CameraComponent.h"
#include "AnimatorComponent.h"
#include "MeshComponent.h"

#include "RenderDevice.h"

#include "q3/Q3BSPAsset.h"
#include "q3/Q3BspMesh.h"
#include "q3/Q3BspCollision.h"

#include "PhysicsCommon.h"

#include <iostream>
#include <fstream>



// Скорее всего переедет в отдельную Entity
static Q3BspMesh g_mesh;
static Q3BspCollision g_collision;

WorldScene::WorldScene()
{
    static bool inited = false;
    
    if (!inited)
    {
        // Временный хак пока не решу куда вставить bsp-меш и коллизию
        loadBSP("assets/wolf/maps/escape2.bsp");
        inited = true;
    }
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

WorldScene::~WorldScene() = default;

void WorldScene::clear()
{
    for (auto& entity : m_entities)
    {
        entity->destroy();
    }
    
    g_UIManager->setEntity(nullptr);
}

void WorldScene::awake()
{
    for (auto& entity : m_entities)
    {
        entity->awake();
    }
}

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

void WorldScene::startSimulation(Viewport* viewport)
{
    m_pGameplayViewport = viewport;
    m_isSimulationMode = true;
    Input::setGameInputRoutingEnabled(true);
    
    // 1. Ищем дефолтную камеру игрока
    for (auto* component : m_allComponents)
    {
        if (auto* cameraComp = dynamic_cast<CameraComponent*>(component))
        {
            if (cameraComp->m_isCurrent)
            {
                m_pActiveCameraComponent = cameraComp;
                
                if (m_pGameplayViewport) {
                    m_pGameplayViewport->setCamera(&cameraComp->m_camera);
                }
                
                break;
            }
        }
    }
    
    // 2. Запускаем старт всех скриптов
    for (auto* component : m_allComponents)
    {
        component->start();
    }
}

void WorldScene::stopSimulation()
{
    m_isSimulationMode = false;
    Input::setGameInputRoutingEnabled(false);
    m_pActiveCameraComponent = nullptr;
    m_pGameplayViewport = nullptr;
}

void WorldScene::prepareRenderViews()
{
    if (!m_isSimulationMode || !m_pGameplayViewport) return;
    
    // === ШАГ А: Системные проходы (Кастомные текстуры, Мониторы, Зеркала) ===
    // Пример: Рендеринг камеры-монитора, установленной на уровне
    /*
     for (auto* monitor : m_uiMonitors) {
     RenderView monitorView;
     monitorView.viewMatrix = monitor->GetView();
     monitorView.projectionMatrix = monitor->GetProj();
     monitorView.targetFrameBuffer = monitor->GetTargetFBO(); // Рендерим в текстуру материала
     monitorView.cullingMask = monitor->layerMask;
     
     collectRenderCommands(monitorView);
     RenderDevice::submitView(monitorView);
     }
     */
    
    // === ШАГ Б: Проходы для карт теней (Shadow Mapping) ===
    /*
     RenderView shadowView;
     shadowView.viewMatrix = glm::lookAt(lightPos, lightTarget, up);
     shadowView.projectionMatrix = glm::ortho(-2048, 2048, -2048, 2048);
     shadowView.targetFrameBuffer = RenderDevice::getShadowMapFBO(); // Кастомный FBO глубины
     shadowView.clearDepthOnly = false;
     shadowView.drawSkybox = false; // Для теней небо не нужно
     shadowView.drawGrid = false;
     
     collectRenderCommands(shadowView);
     RenderDevice::submitView(shadowView);
     */
    
    // === ШАГ В: Игровые камеры (Simulation Mode) ===
    if (m_isSimulationMode)
    {
        std::vector<CameraComponent*> sceneCameras = getCameraComponents();
        std::vector<CameraComponent*> activeCameras;
        
        for(auto* cam : sceneCameras) {
            if (cam->m_isCurrent) activeCameras.push_back(cam);
        }
        
        // Если в режиме симуляции разработчик забыл поставить камеру — падаем на дефолтное поведение редактора
        if (activeCameras.empty())
        {
            m_pActiveCameraComponent = nullptr; // Сигнализируем, что камеры нет
            return; // Управление проходом заберет себе ViewportPanel в preRender()
        }
        
        // Сортируем стек камер по depth (чтобы оверлей оружия рисовался позже мира)
        std::sort(activeCameras.begin(), activeCameras.end(), [](CameraComponent* a, CameraComponent* b) {
            return a->depth < b->depth;
        });
        
        // Создаем проход для каждой камеры
        for (auto* cameraComp : activeCameras)
        {
            // 1. ОПРЕДЕЛЯЕМ ЦЕЛЕВОЙ ВЬЮПОРТ ДЛЯ ЭТОЙ КАМЕРЫ
            // Если у камеры есть свой кастомный вьюпорт (например, для Render-to-Texture монитора),
            // она использует его. Иначе — падает на главный игровой вьюпорт.
            Viewport* targetViewport = cameraComp->m_customViewport ? cameraComp->m_customViewport : m_pGameplayViewport;
            
            // Защита на случай, если вообще никакой вьюпорт не доступен
            if (!targetViewport) continue;
            
            // 2. СИНХРОНИЗАЦИЯ: Камера настраивает свой аспект строго под тот вьюпорт, в который рисует!
            cameraComp->m_camera.setAspectRatio(targetViewport->getAspectRatio());
            
            RenderView gameView;
            
            // 3. Копируем матрицы и параметры из компонента
            gameView.viewMatrix       = cameraComp->m_camera.view;
            gameView.projectionMatrix = cameraComp->m_camera.projection;
            gameView.clearDepthOnly   = cameraComp->clearDepthOnly;
            gameView.cullingMask     = cameraComp->cullingMask;
            gameView.depth           = cameraComp->depth;
            
            // 4. Настройки вывода берем ИМЕННО из целевого вьюпорта
            gameView.targetFrameBuffer = targetViewport->getFrameBuffer();
            gameView.drawSkybox        = targetViewport->drawSkybox;
            
            gameView.drawGrid          = false;
            gameView.drawGizmos        = false;
            
            // 5. Собираем команды сцены
            collectRenderCommands(gameView);
            RenderDevice::submitView(gameView);
        }
    }
}


void WorldScene::collectRenderCommands(RenderView& view)
{
    if (Layers::Static & view.cullingMask)
    {
        g_mesh.draw(view);
    }
    
    for (auto& entity : m_entities)
    {
        if (!entity->m_isActive) continue;
        
        auto meshComp = entity->getComponent<MeshComponent>();
        
        if (meshComp && meshComp->mesh)
        {
            // Проверяем слой конкретного МЕША
            if ((meshComp->layer & view.cullingMask) == 0)
            {
                continue; // Этот меш текущая камера не видит, пропускаем
            }
            
            auto worldMatrix = entity->getTransform().getWorldMatrix();
            
            auto animator = entity->getComponent<AnimatorComponent>();
            
            if (!animator || animator->m_clips.empty())
            {
                meshComp->mesh->draw(view, worldMatrix);
            }
            else
            {
                meshComp->mesh->draw(view, worldMatrix, animator->getFinalBoneMatrices());
            }
        }
    }
}

void WorldScene::selectEntityAtIndex(int index)
{
    if (index < m_entities.size())
    {
        Entity* rawPtr = m_entities[index].get();
        g_UIManager->setEntity(rawPtr);
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

std::vector<CameraComponent*> WorldScene::getCameraComponents()
{
    std::vector<CameraComponent*> cameraComponents;
    
    for (auto& entity : m_entities)
    {
        if (!entity->m_isActive) continue;
        
        entity->getComponents(cameraComponents);
    }
    
    return cameraComponents;
}

void WorldScene::switchCamera(CameraComponent* newCameraComponent)
{
    if (!m_isSimulationMode || !m_pGameplayViewport || !newCameraComponent) return;
    
    // 1. Снимаем флаг со старой камеры
    if (m_pActiveCameraComponent) {
        m_pActiveCameraComponent->m_isCurrent = false;
    }
    
    // 2. Делаем новую камеру активной
    m_pActiveCameraComponent = newCameraComponent;
    m_pActiveCameraComponent->m_isCurrent = true;
    
    // 3. Отдаем Вьюпорту новую математическую камеру!
    m_pGameplayViewport->setCamera(&newCameraComponent->m_camera);
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
