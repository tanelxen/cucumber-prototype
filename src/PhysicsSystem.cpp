//
//  PhysicsSystem.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.10.26.
//

#include "PhysicsSystem.h"
#include "PhysicsContactListener.h"

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>

#include <Jolt/Core/TempAllocator.h>

#include <Jolt/Physics/PhysicsSystem.h>

PhysicsSystem::PhysicsSystem() = default;

PhysicsSystem::~PhysicsSystem()
{
    Shutdown();
}

void PhysicsSystem::Init()
{
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
    
    m_TempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);
    
    m_JobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs,
                                               JPH::cMaxPhysicsBarriers,
                                               static_cast<int>(std::thread::hardware_concurrency()) - 1);
    
    // Jolt works in meters; convert Quake-scale tolerances at the physics boundary.
    JPH::PhysicsSettings settings;
    
    settings.mTimeBeforeSleep = 0.5f;
    settings.mPointVelocitySleepThreshold = 2.0f * QUAKE_UNITS_TO_METERS;
    
    settings.mPenetrationSlop = 0.5f * QUAKE_UNITS_TO_METERS;
    
    // Инициализируем систему базовыми параметрами
    m_physicsSystem.Init(1024, 0, 1024, 1024,
                         m_bpInterface,
                         m_objVsBpFilter,
                         m_objPairFilter);
    
    // Применяем настройки сна и погрешностей к физическому миру
    m_physicsSystem.SetPhysicsSettings(settings);
    
    m_physicsSystem.SetGravity(QuakeToJolt(glm::vec3(0.0f, -800.0f, 0.0f)));
    
    m_contactListener = std::make_unique<PhysicsContactListener>();
    m_physicsSystem.SetContactListener(m_contactListener.get());
}

void PhysicsSystem::Shutdown()
{
    if (m_JobSystem)
    {
        delete m_JobSystem;
        m_JobSystem = nullptr;
    }
    
    if (m_TempAllocator)
    {
        delete m_TempAllocator;
        m_TempAllocator = nullptr;
    }
    
    // Удаляем фабрику при выходе из движка
    if (JPH::Factory::sInstance)
    {
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}

void PhysicsSystem::Update(float dt)
{
    if (m_TempAllocator && m_JobSystem)
    {
        m_physicsSystem.Update(dt, 1, m_TempAllocator, m_JobSystem);
    }
}
