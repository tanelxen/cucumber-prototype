//
//  HitboxComponent.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 28.09.26.
//

#include "CollisionComponent.h"
#include "GizmoRender.h"
#include "Entity.h"
#include "PhysicsSystem.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

CollisionComponent::~CollisionComponent()
{
    DestroyPhysicsBody();
}

void CollisionComponent::awake()
{
    CreatePhysicsBody();
}

void CollisionComponent::save(nlohmann::json& j) const
{
    j = nlohmann::json {
        {"mins", {m_mins.x, m_mins.y, m_mins.z}},
        {"maxs", {m_maxs.x, m_maxs.y, m_maxs.z}},
        {"trigger", isTrigger},
        {"collisionMask", collisionMask},
        {"layer", layer}
    };
}

void CollisionComponent::load(const nlohmann::json& j)
{
    auto mins = j.at("mins").get<std::vector<float>>();
    m_mins = glm::vec3(mins[0], mins[1], mins[2]);
    
    auto maxs = j.at("maxs").get<std::vector<float>>();
    m_maxs = glm::vec3(maxs[0], maxs[1], maxs[2]);
    
    isTrigger = j.at("trigger").get<bool>();
    
    collisionMask = j.at("collisionMask").get<uint32_t>();
    layer = j.at("layer").get<uint32_t>();
}

void CollisionComponent::CreatePhysicsBody()
{
    auto& bodyInterface = PhysicsSystem::Get().GetBodyInterface();
    
    glm::vec3 halfExtents = (m_maxs - m_mins) * 0.5f;
    JPH::BoxShapeSettings shapeSettings(QuakeToJolt(halfExtents));
    JPH::Shape::ShapeResult shapeResult = shapeSettings.Create();
    
    glm::vec3 worldPos = owner->getTransform().getWorldPosition();
    glm::vec3 centerOffset = (m_mins + m_maxs) * 0.5f;
    glm::vec3 finalPos = worldPos + centerOffset;
    
    // ИСПРАВЛЕНИЕ: Выбираем правильный тип движения Jolt
    // Аптечки и триггеры (LAYER_TRIGGER) — Kinematic датчики.
    // Игроки и NPC (PLAYER, ENEMY) — Dynamic объекты, чтобы они расталкивали друг друга
    JPH::EMotionType motionType = JPH::EMotionType::Dynamic;
    
    if (isTrigger || (layer & LAYER_TRIGGER))
    {
        motionType = JPH::EMotionType::Kinematic;
    }
    
    if (layer & LAYER_PLAYER || layer & LAYER_ENEMY)
    {
        motionType = JPH::EMotionType::Kinematic;
    }
    
    // Переводим Cucumber Бит-маску в плоский ObjectLayer для Jolt
    JPH::ObjectLayer objectLayer = Layers::FromCollisionLayer(layer);
    
    JPH::BodyCreationSettings settings(shapeResult.Get(),
                                       QuakeToJoltPosition(finalPos),
                                       JPH::Quat::sIdentity(),
                                       motionType,
                                       objectLayer);
    
    // Если это аптечка/интерактивная зона — включаем режим сенсора
    if (isTrigger || (layer & LAYER_TRIGGER))
    {
        settings.mIsSensor = true;
    }
    else
    {
        // Задаем массу, чтобы враги и игрок могли расталкивать друг друга
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateMassAndInertia;
        settings.mMassPropertiesOverride.mMass = 80.0f; // Дефолтный вес в кг
    }
    
    JPH::Body* body = bodyInterface.CreateBody(settings);
    m_bodyID = body->GetID();
    
    m_userData.entity = owner;
    m_userData.component = this;
    body->SetUserData(reinterpret_cast<JPH::uint64>(&m_userData));
    
    bodyInterface.AddBody(m_bodyID, JPH::EActivation::Activate);
}

void CollisionComponent::DestroyPhysicsBody()
{
    if (!m_bodyID.IsInvalid())
    {
        auto& bodyInterface = PhysicsSystem::Get().GetBodyInterface();
        bodyInterface.RemoveBody(m_bodyID);
        bodyInterface.DestroyBody(m_bodyID);
        m_bodyID = JPH::BodyID();
    }
}

void CollisionComponent::SyncTransformToPhysics()
{
    if (m_bodyID.IsInvalid()) return;
    
    auto& bodyInterface = PhysicsSystem::Get().GetBodyInterface();
    
    glm::vec3 worldPos = owner->getTransform().getWorldPosition();
    glm::vec3 centerOffset = (m_mins + m_maxs) * 0.5f;
    glm::vec3 finalPos = worldPos + centerOffset;
    
    bodyInterface.SetPosition(m_bodyID, QuakeToJoltPosition(finalPos), JPH::EActivation::Activate);
}

void CollisionComponent::SyncPhysicsToTransform()
{
    if (m_bodyID.IsInvalid() || isTrigger) return;
    
    auto& bodyInterface = PhysicsSystem::Get().GetBodyInterface();
    
    JPH::RVec3 pos = bodyInterface.GetPosition(m_bodyID);
    glm::vec3 centerOffset = (m_mins + m_maxs) * 0.5f;
    
    // Возвращаем позицию обратно сущности с учетом смещения центра формы
    glm::vec3 entityPos = JoltToQuakePosition(pos) - centerOffset;
    owner->getTransform().setPosition(entityPos);
}

void CollisionComponent::gizmo()
{
    const glm::vec3& pos = owner->getTransform().getPosition();
    GizmoRender::Get().DrawAABB(pos + m_mins, pos + m_maxs, { 1, 1, 0, 1 });
}
