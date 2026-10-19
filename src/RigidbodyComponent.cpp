//
//  RigidbodyComponent.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 18.10.26.
//

#include "RigidbodyComponent.h"
#include "Entity.h"
#include "PhysicsSystem.h"
#include "GizmoRender.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

void RigidbodyComponent::CreatePhysicsBody()
{
    auto& bodyInterface = PhysicsSystem::Get().GetBodyInterface();
    
    JPH::RefConst<JPH::Shape> JoltShape;
    
    if (shapeType == PhysicsShapeType::Box)
    {
        JPH::BoxShapeSettings boxSettings(QuakeToJolt(boxHalfExtents));
        JoltShape = boxSettings.Create().Get();
    }
    else
    {
        JPH::SphereShapeSettings sphereSettings(sphereRadius * QUAKE_UNITS_TO_METERS);
        JoltShape = sphereSettings.Create().Get();
    }
    
    glm::vec3 worldPos = owner->getTransform().getWorldPosition();
    glm::quat worldRot = getWorldRotation();
    
    JPH::EMotionType joltMotionType = JPH::EMotionType::Dynamic;
    JPH::ObjectLayer joltLayer = Layers::INTERACTIVE;
    
    if (motionType == PhysicsMotionType::Static)
    {
        joltMotionType = JPH::EMotionType::Static;
        joltLayer = Layers::STATIC;
    }
    else if (motionType == PhysicsMotionType::Kinematic)
    {
        joltMotionType = JPH::EMotionType::Kinematic;
        joltLayer = Layers::INTERACTIVE;
    }
    
    JPH::BodyCreationSettings settings(JoltShape,
                                       QuakeToJoltPosition(worldPos),
                                       JPH::Quat(worldRot.x, worldRot.y, worldRot.z, worldRot.w),
                                       joltMotionType,
                                       joltLayer);
    
    if (motionType == PhysicsMotionType::Dynamic)
    {
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateMassAndInertia;
        settings.mMassPropertiesOverride.mMass = mass;
    }
    
    settings.mFriction = friction;
    settings.mRestitution = restitution;
    // settings.mAllowSleeping = false;
    
    JPH::Body* body = bodyInterface.CreateBody(settings);
    m_bodyID = body->GetID();
    
    m_userData.entity = owner;
    m_userData.component = this;
    body->SetUserData(reinterpret_cast<JPH::uint64>(&m_userData));
    
    JPH::EActivation activation = (motionType == PhysicsMotionType::Static) ? JPH::EActivation::DontActivate : JPH::EActivation::Activate;
    bodyInterface.AddBody(m_bodyID, activation);
}

void RigidbodyComponent::DestroyPhysicsBody()
{
    if (!m_bodyID.IsInvalid())
    {
        auto& bodyInterface = PhysicsSystem::Get().GetBodyInterface();
        bodyInterface.RemoveBody(m_bodyID);
        bodyInterface.DestroyBody(m_bodyID);
        m_bodyID = JPH::BodyID();
    }
}

void RigidbodyComponent::SyncTransformToPhysics()
{
//    if (m_bodyID.IsInvalid()) return;
//    auto& bodyInterface = PhysicsSystem::Get().GetBodyInterface();
//    
//    glm::vec3 worldPos = owner->getTransform().getWorldPosition();
//    glm::quat worldRot = getWorldRotation();
//    
//    bodyInterface.SetPositionAndRotation(m_bodyID,
//                                         JPH::RVec3(worldPos.x, worldPos.y, worldPos.z),
//                                         JPH::Quat(worldRot.x, worldRot.y, worldRot.z, worldRot.w),
//                                         JPH::EActivation::Activate);
}

void RigidbodyComponent::SyncPhysicsToTransform()
{
    // Статичные объекты физика сама не двигает, их синхронизировать обратно не нужно
    if (m_bodyID.IsInvalid() || motionType == PhysicsMotionType::Static) return;
    
    auto& bodyInterface = PhysicsSystem::Get().GetBodyInterface();
    
    JPH::RVec3 pos = bodyInterface.GetPosition(m_bodyID);
    JPH::Quat rot = bodyInterface.GetRotation(m_bodyID);
    
    // Передаем координаты обратно в трансформ сущности движка
    owner->getTransform().setPosition(JoltToQuakePosition(pos));
//    owner->getTransform().setRotation(glm::quat(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ()));
    // Внимание: проверьте порядок аргументов вашего конструктора кватернионов (W,X,Y,Z или X,Y,Z,W)
}

glm::quat RigidbodyComponent::getWorldRotation()
{
    glm::mat4 worldMatrix = owner->getTransform().getWorldMatrix();
    
    // Нормализуем колонки матрицы от масштабирования
    glm::mat3 rotMat;
    rotMat[0] = glm::normalize(glm::vec3(worldMatrix[0]));
    rotMat[1] = glm::normalize(glm::vec3(worldMatrix[1]));
    rotMat[2] = glm::normalize(glm::vec3(worldMatrix[2]));
    
    return glm::quat_cast(rotMat);
}

void RigidbodyComponent::gizmo()
{
    const glm::vec3& pos = owner->getTransform().getPosition();
    GizmoRender::Get().DrawAABB(pos + boxHalfExtents, pos - boxHalfExtents, { 1, 1, 0, 1 });
}

void RigidbodyComponent::addImpulse(const glm::vec3& force, const glm::vec3& point)
{
    JPH::BodyInterface& bodyInterface = PhysicsSystem::Get().GetJolt().GetBodyInterface();
    
    if (m_bodyID.IsInvalid() || motionType != PhysicsMotionType::Dynamic) return;

    bodyInterface.ActivateBody(m_bodyID);
    bodyInterface.AddImpulse(m_bodyID, QuakeToJolt(force));
}
