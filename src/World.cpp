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
#include "LightComponent.h"
#include "MeshComponent.h"
#include "Q3BspComponent.h"
#include "MapComponent.h"
#include "RigidbodyComponent.h"

#include "q3/Q3BspMesh.h"
#include "trenchbroom/MapMesh.h"

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
    
    g_UIManager->setEntity(nullptr); // HACK
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
    m_cameras.rebuild();
}

void WorldScene::startSimulation()
{
    m_isSimulationMode = true;
    Input::setGameInputRoutingEnabled(true);
    
    m_cameras.rebuild();
    
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
    
    if (auto* camera = dynamic_cast<CameraComponent*>(component)) {
        m_cameras.add(camera);
    }
    
    m_allComponents.push_back(component);
    
//    if (auto* bsp = dynamic_cast<Q3BspComponent*>(component))
//    {
//        m_bsp = bsp;
//    }
    
    if (auto* map = dynamic_cast<MapComponent*>(component))
    {
        m_map = map;
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
    
    if (auto* camera = dynamic_cast<CameraComponent*>(component)) {
        m_cameras.remove(camera);
    }
    
//    if (auto* bsp = dynamic_cast<Q3BspComponent*>(component))
//    {
//        m_bsp = nullptr;
//    }
    
    if (auto* map = dynamic_cast<MapComponent*>(component))
    {
        m_map = nullptr;
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
    JPH::RRayCast joltRay(QuakeToJoltPosition(start), QuakeToJolt(rayVector));
    
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
