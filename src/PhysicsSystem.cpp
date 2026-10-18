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
    
    // --- НАСТРОЙКА СИСТЕМЫ ЮНИТОВ QUAKE ДЛЯ JOLT 5.6.0 ---
    JPH::PhysicsSettings settings;
    
    // Линейная и угловая скорость, ниже которой объект считает улегшимся спать (Sleeping)
    // Поднимаем значения, так как числа в юнитах Quake значительно больше метров
    settings.mTimeBeforeSleep = 0.5f;
    settings.mPointVelocitySleepThreshold = 2.0f; // 2 юнита в секунду вместо 0.03 метра
    
    // Допустимое проникновение объектов друг в друга до начала выталкивания (по умолч. 0.02м)
    // Для Quake это примерно 0.5 - 1.0 юнит
    settings.mPenetrationSlop = 0.5f;
    
    // Инициализируем систему базовыми параметрами
    m_physicsSystem.Init(1024, 0, 1024, 1024,
                         m_bpInterface,
                         m_objVsBpFilter,
                         m_objPairFilter);
    
    // Применяем настройки сна и погрешностей к физическому миру
    m_physicsSystem.SetPhysicsSettings(settings);
    
    // Устанавливаем классическую гравитацию Quake 3 (800 юнитов/сек^2 вниз по оси Y)
    m_physicsSystem.SetGravity(JPH::Vec3(0.0f, -800.0f, 0.0f));
    
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
