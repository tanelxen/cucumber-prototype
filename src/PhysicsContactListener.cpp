//
//  PhysicsContactListener.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 18.10.26.
//

#include "PhysicsContactListener.h"

#include "PhysicsSystem.h"
#include "CollisionComponent.h"

void PhysicsContactListener::OnContactAdded(const JPH::CharacterVirtual* inCharacter,
                                            const JPH::CharacterContact& inContact,
                                            JPH::CharacterContactSettings& ioSettings)
{
    const JPH::BodyLockInterface& lockInterface = PhysicsSystem::Get().GetJolt().GetBodyLockInterface();
    const JPH::Body* body2 = lockInterface.TryGetBody(inContact.mBodyB);
    
    if(body2 == nullptr) return;
    
    // Если объект, с которым наступил контакт — это сенсор (наша аптечка)
    if (body2->IsSensor())
    {
        auto* userData2 = reinterpret_cast<PhysicsUserData*>(body2->GetUserData());
        if (userData2 && userData2->component)
        {
            auto* collisionComp = static_cast<CollisionComponent*>(userData2->component);
            
            // Получаем сущность игрока из UserData самого CharacterVirtual
            auto* characterUserData = reinterpret_cast<PhysicsUserData*>(inCharacter->GetUserData());
            if (characterUserData && characterUserData->entity)
            {
                // Вызываем коллбек: Игрок вошел в зону аптечки!
                collisionComp->CallTriggerEnter(characterUserData->entity);
            }
        }
    }
    
    // Проверяем, является ли объект динамическим (например, ящик, бочка)
    // Статичные объекты (стены, пол) мы толкать не можем
    else if (body2->GetMotionType() == JPH::EMotionType::Dynamic)
    {
        ioSettings.mCanPushCharacter = false;
        
        JPH::BodyInterface& bodyInterface = PhysicsSystem::Get().GetJolt().GetBodyInterface();
        bodyInterface.ActivateBody(inContact.mBodyB);
        
        // Получаем текущую скорость персонажа
        JPH::Vec3 characterVelocity = inCharacter->GetLinearVelocity();
        
        // Если персонаж движется в сторону объекта
        if (!characterVelocity.IsNearZero())
        {
            // Направление толчка (инвертированная нормаль контакта часто направлена от персонажа к объекту,
            // либо можно использовать направление скорости персонажа)
            JPH::Vec3 pushDirection = -inContact.mContactNormal;
            pushDirection.SetY(0.0f); // Опционально: убираем вертикальную составляющую, чтобы ящики не взлетали
            
            if (!pushDirection.IsNearZero())
            {
                pushDirection = pushDirection.Normalized();
                
                // Сила толчка. Подберите значение под массу ваших ящиков
                float pushForce = 1000.0f;
                JPH::Vec3 impulse = pushDirection * pushForce;
                
                bodyInterface.AddImpulse(inContact.mBodyB, impulse, inContact.mPosition);
            }
        }
    }
}

void PhysicsContactListener::OnContactRemoved(const JPH::CharacterVirtual* inCharacter,
                                              const JPH::BodyID& inBodyID2,
                                              const JPH::SubShapeID& inSubShapeID2)
{
    const JPH::BodyLockInterface& lockInterface = PhysicsSystem::Get().GetJolt().GetBodyLockInterface();
    const JPH::Body* body2 = lockInterface.TryGetBody(inBodyID2);
    
    if (body2 && body2->IsSensor())
    {
        auto* userData2 = reinterpret_cast<PhysicsUserData*>(body2->GetUserData());
        if (userData2 && userData2->component)
        {
            auto* collisionComp = static_cast<CollisionComponent*>(userData2->component);
            auto* characterUserData = reinterpret_cast<PhysicsUserData*>(inCharacter->GetUserData());
            
            if (characterUserData && characterUserData->entity)
            {
                if (collisionComp->onTriggerExit)
                {
                    collisionComp->onTriggerExit(characterUserData->entity);
                }
            }
        }
    }
}
