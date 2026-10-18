//
//  PhysicsSystem.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.10.26.
//

#pragma once

#include "PhysicsCommon.h"
#include <Jolt/Core/JobSystemThreadPool.h>
#include <memory>

class PhysicsContactListener;

class PhysicsSystem
{
public:
    static PhysicsSystem& Get()
    {
        static PhysicsSystem instance;
        return instance;
    }
    
    void Init();
    void Shutdown();
    void Update(float dt);
    
    JPH::PhysicsSystem& GetJolt() { return m_physicsSystem; }
    JPH::BodyInterface& GetBodyInterface() { return m_physicsSystem.GetBodyInterface(); }
    JPH::TempAllocatorImpl* GetTempAllocator() { return m_TempAllocator; };
    PhysicsContactListener* GetContactListener() { return m_contactListener.get(); }
    
private:
    PhysicsSystem();
    ~PhysicsSystem();
    
    JPH::TempAllocatorImpl* m_TempAllocator = nullptr;
    JPH::JobSystemThreadPool* m_JobSystem = nullptr;
    
    BPLayerInterfaceImpl m_bpInterface;
    ObjectVsBroadPhaseLayerFilterImpl m_objVsBpFilter;
    ObjectLayerPairFilterImpl m_objPairFilter;
    
    JPH::PhysicsSystem m_physicsSystem;
    
    std::unique_ptr<PhysicsContactListener> m_contactListener;
};
