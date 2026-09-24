//
//  PhysicsCommon.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 22.09.26.
//

#pragma once
#include <glm/glm.hpp>

class Entity;

//struct PhysicsHitResult {
//    bool hasHit = false;
//    float fraction = 1.0f;
//    glm::vec3 endPosition{0.0f};
//    glm::vec3 normal{0.0f};
//    Entity* hitEntity = nullptr; // Ссылка на игровой объект (платформу/дверь)
//};

struct HitResult
{
    glm::vec3 endpos;
    glm::vec3 normal;
    float fraction;
    
    bool startsolid;
    bool allsolid;
    
    int textureId;
    int surfaceFlags;
    
    Entity* hitEntity = nullptr;
};

inline glm::vec3 OpenGL2Quake(const glm::vec3& in)
{
    return { in.x, -in.z, in.y };
}

inline glm::vec3 Quake2OpenGL(const glm::vec3& in)
{
    return { in.x, in.z, -in.y };
}

/*
class ICharacterCollider {
public:
    virtual ~ICharacterCollider() = default;
    
    // Инициализация (например, создание CharacterVirtual в Jolt или привязка к BSP)
    virtual void Init(Entity* owner, const glm::vec3& mins, const glm::vec3& maxs) = 0;
    
    // Главный метод: принимает желаемую скорость, двигает хитбокс с учетом
    // коллизий (slide/step) и возвращает ИТОГОВУЮ скорость после столкновений со стенами.
    virtual glm::vec3 moveAndSlide(const glm::vec3& velocity, float dt, bool useGravity) = 0;
    
    // Возвращает позицию хитбокса после симуляции
    virtual glm::vec3 getPosition() const = 0;
    
    // Телепортация (например, при спавне или респавне)
    virtual void setPosition(const glm::vec3& position) = 0;
};

class BspCharacterCollider : public ICharacterCollider {
private:
    Entity* m_pOwner = nullptr;
    glm::vec3 m_position;
    glm::vec3 m_mins, m_maxs;
    
public:
    void Init(Entity* owner, const glm::vec3& mins, const glm::vec3& maxs) override {
        m_pOwner = owner;
        m_mins = mins;
        m_maxs = maxs;
        m_position = owner->m_transform.getPosition();
    }
    
    glm::vec3 MoveAndSlide(const glm::vec3& velocity, float dt, bool useGravity) override {
        // Переносим сюда вашу текущую логику:
        // 1. Копируем velocity в глобальную/локальную m_velocity, с которой работает старый step_slide
        // 2. Вызываем ваш старый метод: step_slide(useGravity, dt);
        // 3. step_slide изменяет m_position игрока и m_velocity (при ударе о стены)
        
        // Возвращаем то, что получилось после симуляции коллизий Quake BSP
        return m_velocityAfterStepSlide;
    }
    
    glm::vec3 GetPosition() const override { return m_position; }
    void SetPosition(const glm::vec3& position) override { m_position = position; }
};
 
class JoltCharacterCollider : public ICharacterCollider {
private:
    JPH::CharacterVirtual* m_pJoltCharacter = nullptr;
    // ... ссылки на Jolt Physics System ...
    
public:
    void Init(Entity* owner, const glm::vec3& mins, const glm::vec3& maxs) override {
        // Создаем капсулу или коробку в Jolt на основе mins/maxs
        // Создаем JPH::CharacterVirtualSettings и инициализируем m_pJoltCharacter
        m_pJoltCharacter->SetPosition(toJoltVec3(owner->m_transform.getPosition()));
    }
    
    glm::vec3 MoveAndSlide(const glm::vec3& velocity, float dt, bool useGravity) override {
        // 1. Отдаем квейковскую скорость в Jolt
        m_pJoltCharacter->SetLinearVelocity(toJoltVec3(velocity));
        
        // 2. Jolt сам крутит свои коллизии, слайды по стенам и ступеньки
        JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
        m_pJoltCharacter->ExtendedUpdate(dt, ..., updateSettings);
        
        // Jolt автоматически обработал стояние на движущемся лифте!
        // Нам не нужно было передавать или вычитать m_baseVelocity вручную.
        
        // 3. Возвращаем измененную скорость назад в компонент игрока
        return toGlmVec3(m_pJoltCharacter->GetLinearVelocity());
    }
    
    glm::vec3 GetPosition() const override {
        return toGlmVec3(m_pJoltCharacter->GetPosition());
    }
    
    void SetPosition(const glm::vec3& position) override {
        m_pJoltCharacter->SetPosition(toJoltVec3(position));
    }
};
*/
