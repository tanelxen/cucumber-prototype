//
//  PhysicsContactListener.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 18.10.26.
//

#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

class PhysicsContactListener : public JPH::ContactListener, public JPH::CharacterContactListener
{
public:
//    JPH::ValidateResult OnContactValidate(const JPH::Body& inBody1,
//                                          const JPH::Body& inBody2,
//                                          JPH::RVec3Arg inBaseOffset,
//                                          const JPH::CollideShapeResult& inCollideShapeResult) override;
//    
//    
//    // --- Для обычных тел (Вариант А / Обычная физика) ---
//    void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override;
//    void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override;
    
    // --- Для виртуального персонажа (Вариант Б) ---
    void OnContactAdded(const JPH::CharacterVirtual* inCharacter, const JPH::CharacterContact& inContact, JPH::CharacterContactSettings& ioSettings) override;
    void OnContactRemoved(const JPH::CharacterVirtual *inCharacter, const JPH::BodyID &inBodyID2, const JPH::SubShapeID &inSubShapeID2) override;
    
private:
    void HandleTriggerContact(const JPH::Body& body1, const JPH::Body& body2, bool isEnter);
};
