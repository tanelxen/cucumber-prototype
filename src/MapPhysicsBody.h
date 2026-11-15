//
//  MapPhysicsBody.h
//  Cucumber
//
//  Статическое физическое тело уровня в Jolt: составная форма из выпуклых оболочек солидов.
//  Вынесено из MapComponent: тело знает про Jolt, а компонент нет.
//

#pragma once

#include "PhysicsCommon.h"

#include <Jolt/Physics/Body/BodyID.h>

#include "level/LevelDocument.h"
class Entity;
#include "Component.h"

class MapPhysicsBody
{
public:
    MapPhysicsBody() = default;
    ~MapPhysicsBody() { destroy(); }

    // Адрес m_userData отдаётся в Jolt, поэтому объект нельзя ни копировать, ни переносить
    MapPhysicsBody(const MapPhysicsBody&) = delete;
    MapPhysicsBody& operator=(const MapPhysicsBody&) = delete;

    // Пересоздаёт тело по документу. Пустой уровень: тела нет, результат true.
    // owner и component попадают в PhysicsUserData (для попаданий лучей и контактов).
    bool rebuild(const LevelDocument& level, Entity* owner, Component* component);

    void destroy();

    bool exists() const { return !m_bodyID.IsInvalid(); }

private:
    JPH::BodyID m_bodyID;
    PhysicsUserData m_userData;
};
