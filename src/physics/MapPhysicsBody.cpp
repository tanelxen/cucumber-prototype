//
//  MapPhysicsBody.cpp
//  Cucumber
//

#include "MapPhysicsBody.h"

#include "PhysicsSystem.h"
#include "components/Component.h"

#include "level/LevelCooker.h"
#include "level/LevelDocument.h"

#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include <iostream>
#include <vector>

void MapPhysicsBody::destroy()
{
    if (m_bodyID.IsInvalid()) return;

    auto& bodyInterface = PhysicsSystem::Get().GetBodyInterface();
    bodyInterface.RemoveBody(m_bodyID);
    bodyInterface.DestroyBody(m_bodyID);
    m_bodyID = JPH::BodyID();
}

bool MapPhysicsBody::rebuild(const LevelDocument& level, Entity* owner, Component* component)
{
    destroy();

    std::vector<std::vector<glm::vec3>> hulls;
    LevelCooker::collisionHulls(level, hulls);

    if (hulls.empty()) return true;

    JPH::StaticCompoundShapeSettings compoundSettings;
    JPH::Array<JPH::Vec3> points;
    int rejected = 0;

    for (const auto& hull : hulls)
    {
        points.clear();
        for (const glm::vec3& v : hull) points.push_back(QuakeToJolt(v));

        JPH::ConvexHullShapeSettings convexSettings(points);
        convexSettings.mMaxConvexRadius = 0.01f;

        JPH::Shape::ShapeResult shapeResult = convexSettings.Create();
        if (shapeResult.IsValid())
            compoundSettings.AddShape(JPH::Vec3::sZero(), JPH::Quat::sIdentity(), shapeResult.Get());
        else
            ++rejected;
    }
    if (rejected > 0) std::cout << "[Physics] Jolt rejected " << rejected << " hulls" << std::endl;

    // Финализация физического тела в Jolt
    JPH::Shape::ShapeResult finalShapeResult = compoundSettings.Create();
    if (finalShapeResult.HasError())
    {
        std::cout << "[Physics Error] Map compound shape error: " << finalShapeResult.GetError() << std::endl;
        return false;
    }

    auto& bodyInterface = PhysicsSystem::Get().GetBodyInterface();
    JPH::BodyCreationSettings settings;
    settings.SetShape(finalShapeResult.Get());
    settings.mMotionType = JPH::EMotionType::Static;
    settings.mObjectLayer = Layers::STATIC;
    settings.mFriction = 0.3f;

    JPH::Body* body = bodyInterface.CreateBody(settings);
    m_bodyID = body->GetID();

    m_userData.entity = owner;
    m_userData.component = component;
    bodyInterface.SetUserData(m_bodyID, reinterpret_cast<JPH::uint64>(&m_userData));
    bodyInterface.AddBody(m_bodyID, JPH::EActivation::DontActivate);

    std::cout << "[Physics] Level collision built: " << compoundSettings.mSubShapes.size() << " static brushes." << std::endl;
    return true;
}
