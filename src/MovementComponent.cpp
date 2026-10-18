//
//  MovementComponent.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 20.09.26.
//

#include "MovementComponent.h"
#include "Entity.h"
#include "World.h"
#include "GizmoRender.h"
#include "PhysicsSystem.h"
#include "PhysicsContactListener.h"
#include "CollisionComponent.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp> // Required for length2

#include <algorithm>

static constexpr float cl_movement_accelerate = 15.0f;
static constexpr float cl_movement_airaccelerate = 7.0f;
static constexpr float cl_movement_friction = 8.0f;
static constexpr float cl_stop_speed = 200.0f;

static const glm::vec3 player_mins = { -15.0f, 0.0f, -15.0f };
static const glm::vec3 player_maxs = { 15.0f, 72.0f, 15.0f };

class IgnoreSelfBodyFilter : public JPH::BodyFilter
{
public:
    IgnoreSelfBodyFilter(JPH::BodyID inSelfID) : mSelfID(inSelfID) {}
    bool ShouldCollide(const JPH::BodyID& inBodyID) const override { return inBodyID != mSelfID; }
private:
    JPH::BodyID mSelfID;
};

class IgnoreSelfEntityFilter : public JPH::BodyFilter {
public:
    // Передаем ссылку на саму PhysicsSystem, чтобы иметь доступ к Lock-интерфейсу
    IgnoreSelfEntityFilter(const Entity* inSelfEntity, JPH::PhysicsSystem& inPhysicsSystem)
    : mSelfEntity(inSelfEntity), mPhysicsSystem(inPhysicsSystem) {}
    
    bool ShouldCollide(const JPH::BodyID& inBodyID) const override {
        if (mSelfEntity == nullptr) return true;
        
        // В Jolt получение тела происходит через Lock-интерфейс для безопасного чтения
        // SharedLock обеспечивает многопоточную безопасность, если Jolt работает в фоне
        JPH::BodyLockRead lock(mPhysicsSystem.GetBodyLockInterface(), inBodyID);
        if (lock.Succeeded())
        {
            const JPH::Body& body = lock.GetBody();
            auto* userData = reinterpret_cast<const PhysicsUserData*>(body.GetUserData());
            
            if (userData && userData->entity == mSelfEntity) {
                return false; // Игнорируем тела, принадлежащие этой же сущности
            }
        }
        
        return true;
    }
private:
    const Entity* mSelfEntity;
    JPH::PhysicsSystem& mPhysicsSystem;
};

MovementComponent::MovementComponent() : Component(Flags::RequiresUpdate) {}

MovementComponent::~MovementComponent()
{
    if (m_character) {
        delete m_character;
    }
}

void MovementComponent::awake()
{
    // 1. Создаем форму игрока (Коробка AABB)
    glm::vec3 halfExtents = (player_maxs - player_mins) * 0.5f;
    JPH::BoxShapeSettings boxSettings(JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z));
    
    // Смещаем форму так, чтобы Pivot персонажа был у него в ногах
    JPH::RotatedTranslatedShapeSettings offsetSettings(JPH::Vec3(0, halfExtents.y, 0), JPH::Quat::sIdentity(), boxSettings.Create().Get());
    JPH::RefConst<JPH::Shape> playerShape = offsetSettings.Create().Get();
    
    // 2. Настраиваем виртуального персонажа
    JPH::CharacterVirtualSettings settings;
    settings.mShape = playerShape;
    settings.mMaxSlopeAngle = JPH::DegreesToRadians(45.0f); // Максимальный угол подъема
    settings.mPredictiveContactDistance = 2.0f; // Дистанция сканирования контактов
    
    // 3. Инстанцируем контроллер в мире Jolt
    glm::vec3 startPos = owner->getTransform().getPosition();
    m_character = new JPH::CharacterVirtual(&settings,
                                            JPH::RVec3(startPos.x, startPos.y, startPos.z),
                                            JPH::Quat::sIdentity(),
                                            &PhysicsSystem::Get().GetJolt());
    
    // 4. Привязываем UserData
    m_userData.entity = owner;
    m_userData.component = nullptr;
    m_character->SetUserData(reinterpret_cast<JPH::uint64>(&m_userData));
    m_character->SetListener(PhysicsSystem::Get().GetContactListener());
}

void MovementComponent::addMovementInput(const glm::vec3& direction, float scale)
{
    if (glm::any(glm::isnan(direction)) || glm::any(glm::isinf(direction)) || std::isnan(scale) || std::isinf(scale)) return;
    float lenSq = glm::length2(direction);
    if (lenSq < 0.00001f) return;
    
    glm::vec3 normalizedDir = (lenSq > 1.0f) ? glm::normalize(direction) : direction;
    scale = glm::clamp(scale, -1.0f, 1.0f);
    
    m_inputVector += normalizedDir * scale;
}

void MovementComponent::jump()
{
    if (m_isGrounded) {
        m_wantJump = true;
    }
}

void MovementComponent::launch(const glm::vec3 &launchVelocity, bool overrideCurrent)
{
    if (overrideCurrent) {
        m_velocity = launchVelocity;
    } else {
        m_velocity += launchVelocity;
    }
    if (launchVelocity.y > 0.0f) {
        m_isGrounded = false;
    }
}

void MovementComponent::update(float dt)
{
    if (owner == nullptr || m_character == nullptr) return;
    
    // 1. Считываем состояние земли и скорость, оставшиеся с конца прошлого кадра
    m_isGrounded = m_character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
    
    JPH::Vec3 joltVel = m_character->GetLinearVelocity();
    m_velocity = glm::vec3(joltVel.GetX(), joltVel.GetY(), joltVel.GetZ());
    
    // 2. Обрабатываем трение, гравитацию и прыжок (на основе m_isGrounded)
    HandleGroundAndFriction(dt);
    
    // 3. Рассчитываем горизонтальное ускорение от ввода игрока
    HandleMovementAndAcceleration(dt);
    
    // 4. Передаем измененную скорость обратно в персонажа перед симуляцией шага
    m_character->SetLinearVelocity(JPH::Vec3(m_velocity.x, m_velocity.y, m_velocity.z));
    
    // 5. Настраиваем параметры кадра для преодоления ступеней (заменяет старый STEPSIZE = 18)
    JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
    updateSettings.mWalkStairsStepUp = m_character->GetUp() * 18.0f;
    updateSettings.mStickToFloorStepDown = -m_character->GetUp() * 18.0f;
    
    IgnoreSelfEntityFilter myFilter(owner, PhysicsSystem::Get().GetJolt());
    
    // 6. Вызываем физическое обновление позиции персонажа (с расчетом коллизий со стенами)
    m_character->ExtendedUpdate(dt,
                                PhysicsSystem::Get().GetJolt().GetGravity(),
                                updateSettings,
                                PhysicsSystem::Get().GetJolt().GetDefaultBroadPhaseLayerFilter(Layers::PLAYER),
                                PhysicsSystem::Get().GetJolt().GetDefaultLayerFilter(Layers::PLAYER),
                                myFilter,
                                JPH::ShapeFilter(),
                                *PhysicsSystem::Get().GetTempAllocator());
    
    // 7. Сразу после симуляции забираем итоговую позицию и отдаем её в Transform вашего движка
    JPH::RVec3 finalPos = m_character->GetPosition();
    owner->getTransform().setPosition(glm::vec3(finalPos.GetX(), finalPos.GetY(), finalPos.GetZ()));
    
    // Зануляем ввод для следующего кадра
    m_inputVector = { 0.0f, 0.0f, 0.0f };
    
    if (auto* collision = owner->getComponent<CollisionComponent>())
    {
        // Двигаем хитбокс вслед за CharacterVirtual
        collision->SyncTransformToPhysics();
    }
}

void MovementComponent::HandleGroundAndFriction(float dt)
{
    if (m_isGrounded) {
        // Применяем трение к горизонтальной скорости на земле (как в Q3)
        float speed = glm::length(glm::vec3(m_velocity.x, 0.0f, m_velocity.z));
        if (speed > 0.1f) {
            float control = speed < cl_stop_speed ? cl_stop_speed : speed;
            float drop = control * cl_movement_friction * dt;
            float newSpeed = std::max(0.0f, speed - drop);
            m_velocity.x *= newSpeed / speed;
            m_velocity.z *= newSpeed / speed;
        }
        
        // Обработка прыжка
        if (m_wantJump) {
            m_velocity.y = 270.0f; // Классический импульс прыжка Quake 3
            m_wantJump = false;
            m_isGrounded = false;
        } else {
            m_velocity.y = 0.0f; // Прижимаем к земле, если нет вертикального импульса
        }
    } else {
        // Если в воздухе — Jolt ExtendedUpdate применит гравитацию сам,
        // но для предсказуемости движения добавим падение по оси Y вручную
        JPH::Vec3 gravity = PhysicsSystem::Get().GetJolt().GetGravity();
        m_velocity.y += gravity.GetY() * dt;
    }
}

void MovementComponent::HandleMovementAndAcceleration(float dt)
{
    if (glm::length2(m_inputVector) < 0.0001f) return;
    
    glm::vec3 wishDir = glm::normalize(m_inputVector);
    
    // Вычисляем желаемую скорость кадра
    float currentMaxSpeed = m_forwardspeed;
    float wishSpeed = glm::length(m_inputVector) * currentMaxSpeed;
    wishSpeed = std::min(wishSpeed, currentMaxSpeed);
    
    // Алгоритм акселерации Quake 3
    float acceleration = m_isGrounded ? cl_movement_accelerate : cl_movement_airaccelerate;
    
    float currentSpeed = glm::dot(m_velocity, wishDir);
    float addSpeed = wishSpeed - currentSpeed;
    
    if (addSpeed > 0.0f) {
        float accelSpeed = acceleration * dt * wishSpeed;
        accelSpeed = std::min(accelSpeed, addSpeed);
        m_velocity += wishDir * accelSpeed;
    }
}

void MovementComponent::gizmo()
{
    const glm::vec3& pos = owner->getTransform().getPosition();
    // Отрисовка коробки коллизии игрока в гизмо
    GizmoRender::Get().DrawAABB(pos + player_mins, pos + player_maxs, { 1, 0, 0, 1 });
}
