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
#include "Q3BspComponent.h"
#include "RigidbodyComponent.h"

#include "RenderDevice.h"

#include "q3/Q3BspMesh.h"

#include "PhysicsCommon.h"
#include "PhysicsSystem.h"

#include "ProjectManager.h"

#include <iostream>
#include <fstream>


WorldScene::WorldScene()
{
    
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
        // 1. Шагаем физику Jolt
        PhysicsSystem::Get().Update(dt);
        
        // 2. Итерируемся по всем сущностям, у которых есть RigidbodyComponent,
        // и забираем новые позиции динамических объектов.
        for (auto& entity : m_entities)
        {
            auto* rb = entity->getComponent<RigidbodyComponent>();
            if (rb && rb->enabled) {
                rb->SyncPhysicsToTransform();
            }
        }
        
        // 2. Извлекаем результаты симуляции Jolt обратно в наши Transform компоненты
        for (auto* component : m_globalColliders) {
            if (component && component->enabled && !component->isTrigger) {
                component->SyncPhysicsToTransform();
            }
        }
        
        // 3. Стандартное обновление скриптов/компонентов
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
    if (m_bsp != nullptr && m_bsp->mesh != nullptr && Layers::Static & view.cullingMask)
    {
        m_bsp->mesh->draw(view);
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
        
//        auto bspComp = entity->getComponent<BSPMeshComponent>();
//        
//        if (bspComp && bspComp->mesh)
//        {
//            bspComp->mesh->draw(view);
//        }
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
    
    if (auto* bsp = dynamic_cast<Q3BspComponent*>(component))
    {
        m_bsp = bsp;
    }
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
    
    if (auto* bsp = dynamic_cast<Q3BspComponent*>(component))
    {
        m_bsp = nullptr;
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

#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>

void WorldScene::lineTrace(struct RaycastHit& outHit,
                           const glm::vec3& start,
                           const glm::vec3& direction,
                           float maxDistance,
                           Entity* ignoreEntity,
                           uint32_t mask) const
{
    outHit.bHit = false;
    outHit.fraction = 1.0f;
    outHit.hitPosition = start + direction * maxDistance;
    outHit.hitNormal = -direction;
    outHit.hitEntity = nullptr;
    outHit.hitComponent = nullptr;
    
    glm::vec3 normDir = glm::normalize(direction);
    glm::vec3 rayVector = normDir * maxDistance;
    JPH::RRayCast joltRay(JPH::RVec3(start.x, start.y, start.z), JPH::Vec3(rayVector.x, rayVector.y, rayVector.z));
    
    JPH::ClosestHitCollisionCollector<JPH::CastRayCollector> collector;
    JPH::RayCastSettings raySettings;
    
    // УЛУЧШЕННЫЙ ФИЛЬТР ЛУЧА
    class CucumberBodyFilter : public JPH::BodyFilter
    {
    public:
        uint32_t mMask;
        const Entity* mIgnoreEntity;
        
        CucumberBodyFilter(uint32_t mask, const Entity* ignoreEntity)
        : mMask(mask), mIgnoreEntity(ignoreEntity) {}
        
        bool ShouldCollide(const JPH::BodyID& inBodyID) const override
        {
            JPH::BodyLockRead lock(PhysicsSystem::Get().GetJolt().GetBodyLockInterface(), inBodyID);
            
            if (lock.Succeeded())
            {
                const JPH::Body& body = lock.GetBody();
                
                // 1. СРАЗУ ИГНОРИРУЕМ СТРЕЛКА (или указанную сущность) по UserData
                if (mIgnoreEntity && body.GetUserData() != 0)
                {
                    auto* userData = reinterpret_cast<const PhysicsUserData*>(body.GetUserData());
                    if (userData && userData->entity == mIgnoreEntity)
                    {
                        return false; // Луч пролетит насквозь
                    }
                }
                
                // 2. Проверяем битовую маску слоев
                uint32_t bodyLayerBit = Layers::ToCollisionLayerBit(body.GetObjectLayer());
                return (mMask & bodyLayerBit) != 0;
            }
            
            return false;
        }
    };
    
    // Передаем mask и ignoreEntity в конструктор фильтра
    CucumberBodyFilter filter(mask, ignoreEntity);
    
    PhysicsSystem::Get().GetJolt().GetNarrowPhaseQuery().CastRay(joltRay,
                                                                 raySettings,
                                                                 collector,
                                                                 JPH::BroadPhaseLayerFilter(),
                                                                 JPH::ObjectLayerFilter(),
                                                                 filter);
    
    if (collector.HadHit())
    {
        outHit.fraction = collector.mHit.mFraction;
        outHit.bHit = true;
        outHit.hitPosition = start + normDir * (maxDistance * outHit.fraction);
        
        JPH::BodyLockRead lock(PhysicsSystem::Get().GetJolt().GetBodyLockInterface(), collector.mHit.mBodyID);
        
        if (lock.Succeeded())
        {
            const JPH::Body& body = lock.GetBody();
            
            JPH::Vec3 joltNormal = body.GetWorldSpaceSurfaceNormal(collector.mHit.mSubShapeID2, joltRay.GetPointOnRay(outHit.fraction));
            outHit.hitNormal = glm::vec3(joltNormal.GetX(), joltNormal.GetY(), joltNormal.GetZ());
            
            if (body.GetUserData() != 0)
            {
                auto* userData = reinterpret_cast<PhysicsUserData*>(body.GetUserData());
                if (userData)
                {
                    outHit.hitEntity = userData->entity;
                    outHit.hitComponent = userData->component;
                }
            }
        }
    }
}

