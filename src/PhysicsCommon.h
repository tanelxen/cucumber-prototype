//
//  PhysicsCommon.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 22.09.26.
//

#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

#include <glm/glm.hpp>

class Entity;
class Component;

struct PhysicsUserData {
    Entity* entity = nullptr;
    Component* component = nullptr;
};

enum CollisionLayer : uint32_t
{
    LAYER_PLAYER        = 1 << 0, // Игрок
    LAYER_ENEMY         = 1 << 1, // Все NPC
    LAYER_PROJECTILE    = 1 << 2, // Летящие ракеты, плазма, гранаты
    LAYER_TRIGGER       = 1 << 3, // Интерактивные зоны (аптечки, триггерные двери, зоны урона)
    LAYER_INTERACTIVE   = 1 << 4, // Двигающиеся платформы, разрушаемые ящики, двери-сущности
    LAYER_STATIC        = 1 << 5  // Статичная BSP карта мира
};

namespace PhysicsMask
{
    const uint32_t PlayerMovement = LAYER_ENEMY | LAYER_INTERACTIVE;
    const uint32_t NPCMovement    = LAYER_PLAYER | LAYER_ENEMY | LAYER_INTERACTIVE;
    const uint32_t Shooting       = LAYER_PLAYER | LAYER_ENEMY | LAYER_INTERACTIVE;
    const uint32_t NPCSight       = LAYER_PLAYER | LAYER_INTERACTIVE;
    const uint32_t PlayerRocket   = LAYER_ENEMY | LAYER_INTERACTIVE;
}

struct RaycastHit
{
    bool bHit = false;             // Было ли попадание?
    float fraction = 1.0f;         // Доля пути луча (от 0.0 до 1.0)
    glm::vec3 hitPosition{0.0f};   // Точка пересечения в мировых координатах движка
    glm::vec3 hitNormal{0.0f};     // Нормаль поверхности в точке попадания
    Entity* hitEntity = nullptr;   // Сущность, в которую попали
    Component* hitComponent = nullptr; // Компонент коллизии, если попали в динамику
};

// Преобразуем битовые слои в плоские индексы Jolt (Object Layers)
namespace Layers {
    static constexpr JPH::ObjectLayer STATIC      = 0;
    static constexpr JPH::ObjectLayer PLAYER      = 1;
    static constexpr JPH::ObjectLayer ENEMY       = 2;
    static constexpr JPH::ObjectLayer PROJECTILE  = 3;
    static constexpr JPH::ObjectLayer TRIGGER     = 4;
    static constexpr JPH::ObjectLayer INTERACTIVE = 5;
    static constexpr JPH::ObjectLayer NUM_LAYERS  = 6;
    
    // Вспомогательные функции перевода
    inline JPH::ObjectLayer FromCollisionLayer(uint32_t layerBit) {
        if (layerBit & LAYER_PLAYER)      return PLAYER;
        if (layerBit & LAYER_ENEMY)       return ENEMY;
        if (layerBit & LAYER_PROJECTILE)  return PROJECTILE;
        if (layerBit & LAYER_TRIGGER)     return TRIGGER;
        if (layerBit & LAYER_INTERACTIVE) return INTERACTIVE;
        return STATIC;
    }
    
    inline uint32_t ToCollisionLayerBit(JPH::ObjectLayer layer) {
        switch (layer) {
            case PLAYER:      return LAYER_PLAYER;
            case ENEMY:       return LAYER_ENEMY;
            case PROJECTILE:  return LAYER_PROJECTILE;
            case TRIGGER:     return LAYER_TRIGGER;
            case INTERACTIVE: return LAYER_INTERACTIVE;
            default:          return LAYER_STATIC;
        }
    }
    
    // Дефолтные маски столкновений
    inline uint32_t GetDefaultMask(JPH::ObjectLayer layer) {
        switch (layer) {
            case STATIC:      return LAYER_PLAYER | LAYER_ENEMY | LAYER_PROJECTILE | LAYER_INTERACTIVE;
            case PLAYER:      return LAYER_STATIC | LAYER_ENEMY | LAYER_PROJECTILE | LAYER_TRIGGER | LAYER_INTERACTIVE;
            case ENEMY:       return LAYER_STATIC | LAYER_PLAYER | LAYER_PROJECTILE | LAYER_TRIGGER | LAYER_INTERACTIVE;
            case PROJECTILE:  return LAYER_STATIC | LAYER_PLAYER | LAYER_ENEMY | LAYER_INTERACTIVE;
            case TRIGGER:     return LAYER_PLAYER;
            case INTERACTIVE: return LAYER_STATIC | LAYER_PLAYER | LAYER_ENEMY | LAYER_PROJECTILE | LAYER_INTERACTIVE;
            default:          return 0xFFFFFFFF;
        }
    }
}

namespace BroadPhaseLayers {
    static constexpr JPH::BroadPhaseLayer STATIC(0);
    static constexpr JPH::BroadPhaseLayer DYNAMIC(1);
    static constexpr JPH::BroadPhaseLayer TRIGGER(2);
    static constexpr uint32_t NUM_LAYERS = 3;
}

class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        mObjectToBroadPhase[Layers::STATIC]      = BroadPhaseLayers::STATIC;
        mObjectToBroadPhase[Layers::PLAYER]      = BroadPhaseLayers::DYNAMIC;
        mObjectToBroadPhase[Layers::ENEMY]       = BroadPhaseLayers::DYNAMIC;
        mObjectToBroadPhase[Layers::PROJECTILE]  = BroadPhaseLayers::DYNAMIC;
        mObjectToBroadPhase[Layers::TRIGGER]     = BroadPhaseLayers::TRIGGER;
        mObjectToBroadPhase[Layers::INTERACTIVE] = BroadPhaseLayers::DYNAMIC;
    }
    uint32_t GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override { return mObjectToBroadPhase[inLayer]; }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
        if (inLayer == BroadPhaseLayers::STATIC) return "STATIC";
        if (inLayer == BroadPhaseLayers::DYNAMIC) return "DYNAMIC";
        if (inLayer == BroadPhaseLayers::TRIGGER) return "TRIGGER";
        return "INVALID";
    }
#endif
private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

// Фильтр: какие слои могут сталкиваться друг с другом
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const override {
        uint32_t bit1 = Layers::ToCollisionLayerBit(inLayer1);
        uint32_t bit2 = Layers::ToCollisionLayerBit(inLayer2);
        
        // Проверяем, разрешено ли слою 1 сталкиваться с битовой маской слоя 2, и наоборот
        return (Layers::GetDefaultMask(inLayer1) & bit2) && (Layers::GetDefaultMask(inLayer2) & bit1);
    }
};

class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
        if (inLayer1 == Layers::STATIC) return inLayer2 == BroadPhaseLayers::DYNAMIC;
        if (inLayer2 == BroadPhaseLayers::STATIC) return inLayer1 != Layers::TRIGGER;
        if (inLayer1 == Layers::TRIGGER) return inLayer2 == BroadPhaseLayers::DYNAMIC;
        return true;
    }
};
