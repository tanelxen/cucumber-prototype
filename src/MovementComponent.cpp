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

#include "PhysicsCommon.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp> // Required for length2

static constexpr float cl_forwardspeed = 400.0;
static constexpr float cl_sidespeed = 350.0;
static constexpr float cl_movement_accelerate = 15.0;
static constexpr float cl_movement_airaccelerate = 7.0;
static constexpr float cl_movement_friction = 8.0;
static constexpr float sv_gravity = 800.0;
static constexpr float sv_max_speed = 320.0;
static constexpr float cl_stop_speed = 200.0;
static constexpr float cpm_air_stop_acceleration = 2.5;
static constexpr float cpm_air_control_amount = 150.0;
static constexpr float cpm_strafe_acceleration = 70.0;
static constexpr float cpm_wish_speed = 60.0;

static constexpr int MAX_CLIP_PLANES = 5;
static constexpr float OVERCLIP = 1.001;
static constexpr float STEPSIZE = 18;

static glm::vec3 player_mins = { -15, -15, 0 };
static glm::vec3 player_maxs = { 15, 15, 56 };

enum movement_bits
{
    MOVEMENT_JUMP = 1<<1,
    MOVEMENT_JUMP_THIS_FRAME = 1<<2,
    MOVEMENT_JUMPING = 1<<3,
    LAST_MOVEMENT_BIT
};

static bool noclip = false;

static void clip_velocity(const glm::vec3& in, const glm::vec3& normal, glm::vec3& out, float overbounce);

MovementComponent::MovementComponent() : Component(Flags::RequiresUpdate)
{
    
}

void MovementComponent::gizmo()
{
    const glm::vec3& pos = owner->getTransform().getPosition();
    GizmoRender::Get().DrawAABB(pos + Quake2OpenGL(player_mins), pos + Quake2OpenGL(player_maxs), { 1, 0, 0, 1 });
}

//void MovementComponent::setInput(float forward, float right, bool jump)
//{
//
//}

void MovementComponent::addMovementInput(const glm::vec3& direction, float scale)
{
//    m_inputVector += OpenGL2Quake(direction) * scale;
    
    // 1. Защита от NaN и бесконечностей (ввод может прийти из забагованного скрипта)
    if (glm::any(glm::isnan(direction)) || glm::any(glm::isinf(direction)) ||
        std::isnan(scale) || std::isinf(scale)) {
        return;
    }
    
    // 2. Защита от нулевого вектора (нет смысла обрабатывать и нормализовать)
    float lenSq = glm::length2(direction); // Используем квадрат длины для скорости
    if (lenSq < 0.00001f) {
        return;
    }
    
    // В Unreal ввод нормализуется, если передан неединичный вектор
    glm::vec3 normalizedDir = (lenSq > 1.0f) ? glm::normalize(direction) : direction;
    
    // 3. Ограничиваем масштаб (в Unreal масштаб обычно зажат между -1.0 и 1.0)
    scale = glm::clamp(scale, -1.0f, 1.0f);
    
    // Накапливаем безопасный вектор ввода
    m_inputVector += OpenGL2Quake(normalizedDir) * scale;
}

void MovementComponent::jump()
{
    m_movementBits |= MOVEMENT_JUMP;
}

void MovementComponent::launch(const glm::vec3 &launchVelocity, bool overrideCurrent)
{
    if (overrideCurrent) {
        m_velocity = launchVelocity; // Жестко перезаписываем (для телепортов/батутов)
    } else {
        m_velocity += launchVelocity; // Добавляем импульс (для взрывов/рывков)
    }
    
    // Принудительно отрываем от земли, если импульс направлен вверх
    if (launchVelocity.z > 0.0f) {
        m_isGrounded = false;
    }
}

void MovementComponent::update(float dt)
{
    if (owner == nullptr) return;
    
    m_position = OpenGL2Quake(owner->getTransform().getPosition());
    m_forward = OpenGL2Quake(owner->getTransform().getForward());
    m_right = OpenGL2Quake(owner->getTransform().getRight());
    
    trace_ground();
    apply_inputs(dt);
    
    bool useGravity = (m_movementBits & MOVEMENT_JUMPING) != 0;
    step_slide(useGravity, dt);
    
    m_movementBits &= ~MOVEMENT_JUMP_THIS_FRAME;
    
    // В конце кадра зануляем ввод
    m_inputVector = { 0.0f, 0.0f, 0.0f };
    m_movementBits = 0;
    
    owner->getTransform().setPosition(Quake2OpenGL(m_position));
}

void MovementComponent::trace_ground()
{
    glm::vec3 point = m_position;
    point.z -= 0.25;
    
    HitResult result;
    g_currentScene->trace(result, m_position, point, player_mins, player_maxs);
    
    if (result.fraction == 1.0f || (m_movementBits & MOVEMENT_JUMP_THIS_FRAME))
    {
        m_movementBits |= MOVEMENT_JUMPING;
        m_groundNormal = {0, 0, 0};
        m_surfaceFlags = 0;
        m_isGrounded = false;
    }
    else
    {
        m_movementBits &= ~MOVEMENT_JUMPING;
        m_groundNormal = result.normal;
        m_surfaceFlags = result.surfaceFlags;
        m_isGrounded = true;
    }
}

void MovementComponent::apply_inputs(float dt)
{
    apply_jump();
    apply_friction(dt);
    
    if (glm::length2(m_inputVector) < 0.0001f) {
        return; // Игрок никуда не жмет
    }
    
    // 1. Разделяем вектор ввода на направление и интенсивность нажатия
    float inputLength = glm::length(m_inputVector);
    glm::vec3 wishDir = m_inputVector / inputLength; // Нормализованное мировое направление
    
    // 2. Получаем локальные оси взгляда персонажа из Transform
    glm::vec3 forward = m_forward;
    glm::vec3 right = m_right;
    
    forward.z = 0.0f; right.z = 0.0f; // Проецируем на плоскость земли
    forward = glm::normalize(forward);
    right = glm::normalize(right);
    
    // 3. Проецируем желаемое направление движения на оси персонажа
    // dot-product покажет, насколько движение "прямое" или "боковое" (от -1.0 до 1.0)
    float forwardComponent = std::abs(glm::dot(wishDir, forward));
    float strafeComponent = std::abs(glm::dot(wishDir, right));
    
    // 4. Задаем разные настройки максимальной скорости (вынесите их в структуру настроек)
    float maxForwardSpeed = cl_forwardspeed; // Скорость бега вперед
    float maxStrafeSpeed = cl_sidespeed;  // Скорость стрейфа (меньше, чем вперед)
    
    // Интерполируем максимальную скорость в зависимости от направления движения.
    // Если игрок идет строго по диагонали, скорость будет средней между ними.
    float totalComponent = forwardComponent + strafeComponent;
    float currentMaxSpeed = maxForwardSpeed;
    
    if (totalComponent > 0.0f) {
        currentMaxSpeed = ((forwardComponent * maxForwardSpeed) + (strafeComponent * maxStrafeSpeed)) / totalComponent;
    }
    
    // 5. Финальная желаемая скорость кадра
    float wishSpeed = inputLength * currentMaxSpeed;
    wishSpeed = std::min(wishSpeed, currentMaxSpeed); // Гарантируем ограничение
    
    // 6. Передаем рассчитанные wishDir и wishSpeed в стандартный акселейт Quake 3
    float acceleration = (m_isGrounded) ? cl_movement_accelerate : cl_movement_airaccelerate;
    apply_acceleration(wishDir, wishSpeed, acceleration, dt);
}


void MovementComponent::apply_air_control(const glm::vec3& direction, float wishspeed, float dt)
{
    if (wishspeed == 0) return;
    
    float falling_speed = m_velocity.z;
    m_velocity.z = 0;
    
    float speed = sqrt(glm::dot(m_velocity, m_velocity));
    
    if (speed >= 0.0001f) {
        m_velocity /= speed;
    }
    
    float dot = glm::dot(m_velocity, direction);
    
    if (dot > 0)
    {
        m_velocity *= speed;
        m_velocity = glm::normalize(m_velocity);
    }
    
    m_velocity *= speed;
    m_velocity.z = falling_speed;
}

void MovementComponent::apply_acceleration(const glm::vec3& direction, float wishspeed, float acceleration, float dt)
{
    if (!noclip && (m_movementBits & MOVEMENT_JUMPING)) {
        wishspeed = fmin(cpm_wish_speed, wishspeed);
    }
    
    float cur_speed = glm::dot(m_velocity, direction);
    float add_speed = wishspeed - cur_speed;
    
    if (add_speed <= 0) return;
    
    float accel_speed = acceleration * dt * wishspeed;
    accel_speed = fmin(accel_speed, add_speed);
    
    m_velocity += direction * accel_speed;
}

void MovementComponent::apply_friction(float dt)
{
    if (!noclip)
    {
        if ((m_movementBits & MOVEMENT_JUMPING) || (m_movementBits & MOVEMENT_JUMP_THIS_FRAME)) return;
    }
    
    float speed = sqrt(glm::dot(m_velocity, m_velocity));
    
    if (speed < 1) {
        m_velocity.x = 0;
        m_velocity.y = 0;
        return;
    }
    
    float control = speed < cl_stop_speed ? cl_stop_speed : speed;
    float new_speed = speed - control * cl_movement_friction * dt;
    
    m_velocity *= fmax(0, new_speed) / speed;
}

void MovementComponent::apply_jump()
{
    if (!(m_movementBits & MOVEMENT_JUMP)) return;
    if ((m_movementBits & MOVEMENT_JUMPING) && !noclip) return;
    
    m_movementBits |= MOVEMENT_JUMP_THIS_FRAME;
    m_movementBits &= ~MOVEMENT_JUMP;
    
    m_velocity.z = 270;
}

void MovementComponent::step_slide(bool gravity, float dt)
{
    glm::vec3 start_o = m_position;
    glm::vec3 start_v = m_velocity;
    
    if (!slide(gravity, dt))
    {
        // we got exactly where we wanted to go first try
        return;
    }
    
    glm::vec3 down = start_o;
    down.z -= STEPSIZE;
    
    HitResult result;
    g_currentScene->trace(result, start_o, down, player_mins, player_maxs);
    
    glm::vec3 up = {0, 0, 1};
    
    // never step up when you still have up velocity
    if (m_velocity.z > 0 && (result.fraction == 1.0 || glm::dot(result.normal, up) < 0.7))
    {
        return;
    }
    
    up = start_o;
    up.z += STEPSIZE;
    
    // test the player position if they were a stepheight higher
    g_currentScene->trace(result, up, up, player_mins, player_maxs);
    
    if (result.allsolid)
    {
        // can't step up
        return;
    }
    
    // try slidemove from this position
    m_position = up;
    m_velocity = start_v;
    
    slide(gravity, dt);
    
    // push down the final amount
    down = m_position;
    down.z -= STEPSIZE;
    
    g_currentScene->trace(result, m_position, down, player_mins, player_maxs);
    
    if (!result.allsolid)
    {
        m_position = result.endpos;
    }
    
    if (result.fraction < 1.0)
    {
        clip_velocity(m_velocity, result.normal, m_velocity, OVERCLIP);
    }
}

bool MovementComponent::slide(bool gravity, float dt)
{
    glm::vec3 end_velocity;
    glm::vec3 planes[MAX_CLIP_PLANES];
    
    if (gravity)
    {
        end_velocity = m_velocity;
        end_velocity.z -= sv_gravity * dt;
        
        /*
         * not 100% sure why this is necessary, maybe to avoid tunneling
         * through the floor when really close to it
         */
        
        m_velocity.z = (end_velocity.z + m_velocity.z) * 0.5f;
        
        /* slide against floor */
        if (m_isGrounded) {
            clip_velocity(m_velocity, m_groundNormal, m_velocity, OVERCLIP);
        }
    }
    
    int n_planes = 0;
    
    if (m_isGrounded) {
        planes[n_planes] = m_groundNormal;
        ++n_planes;
    }
    
    planes[n_planes] = glm::normalize(m_velocity);
    ++n_planes;
    
    int n_bumps;
    float time_left = dt;
    
    for (n_bumps = 0; n_bumps < 4; ++n_bumps)
    {
        /* calculate future position and attempt the move */
        HitResult work;
        glm::vec3 end = m_position + m_velocity * time_left;
        g_currentScene->trace(work, m_position, end, player_mins, player_maxs);
        
        //        printf("(fraction = %.1f) (allsolid = %i) \n", work.fraction, work.allsolid);
        
        if (work.allsolid)
        {
            // entity is completely trapped in another solid
            // don't build up falling damage, but allow sideways acceleration
            m_velocity.z = 0;
            return false;
        }
        
        if (work.fraction > 0) {
            m_position = work.endpos;
        }
        
        /* if nothing blocked us we are done */
        if (work.fraction == 1) {
            break;
        }
        
        time_left -= time_left * work.fraction;
        
        if (n_planes >= MAX_CLIP_PLANES) {
            m_velocity = {0, 0, 0};
            return false;
        }
        
        /*
         * if it's a plane we hit before, nudge velocity along it
         * to prevent epsilon issues and dont re-test it
         */
        
        int i;
        
        for (i = 0; i < n_planes; ++i)
        {
            if (glm::dot(work.normal, planes[i]) > 0.99) {
                m_velocity += work.normal;
                break;
            }
        }
        
        if (i < n_planes) {
            continue;
        }
        
        /*
         * entirely new plane, add it and clip velocity against all
         * planes that the move interacts with
         */
        
        planes[n_planes] = work.normal;
        ++n_planes;
        
        for (i = 0; i < n_planes; ++i)
        {
            if (glm::dot(m_velocity, planes[i]) >= 0.1) {
                continue;
            }
            
            glm::vec3 clipped;
            glm::vec3 end_clipped;
            
            clip_velocity(m_velocity, planes[i], clipped, OVERCLIP);
            clip_velocity(end_velocity, planes[i], end_clipped, OVERCLIP);
            
            /*
             * if the clipped move still hits another plane, slide along
             * the line where the two planes meet (cross product) with the
             * un-clipped velocity
             *
             * TODO: reduce nesting in here
             */
            
            for (int j = 0; j < n_planes; ++j)
            {
                if (j == i) {
                    continue;
                }
                
                if (glm::dot(clipped, planes[j]) >= 0.1) {
                    continue;
                }
                
                clip_velocity(clipped, planes[j], clipped, OVERCLIP);
                clip_velocity(end_clipped, planes[j], end_clipped, OVERCLIP);
                
                if (glm::dot(clipped, planes[i]) >= 0) {
                    /* goes back into the first plane */
                    continue;
                }
                
                glm::vec3 dir = glm::normalize(glm::cross(planes[i], planes[j]));
                
                clipped = dir * glm::dot(dir, m_velocity);
                end_clipped = dir * glm::dot(dir, end_velocity);
                
                
                /* if we still hit a plane, just give up and dead stop */
                
                for (int k = 0; k < n_planes; ++k)
                {
                    if (k == j || k == i) {
                        continue;
                    }
                    
                    if (glm::dot(clipped, planes[k]) >= 0.1) {
                        continue;
                    }
                    
                    m_velocity = {0, 0, 0};
                    return false;
                }
            }
            
            /* resolved all collisions for this move */
            m_velocity = clipped;
            end_velocity = end_clipped;
            break;
        }
    }
    
    if (gravity) {
        m_velocity = end_velocity;
    }
    
    return n_bumps > 0;
}


static void clip_velocity(const glm::vec3& in, const glm::vec3& normal, glm::vec3& out, float overbounce)
{
    float backoff = glm::dot(in, normal);
    
    if (backoff < 0) {
        backoff *= overbounce;
    } else {
        backoff /= overbounce;
    }
    
    out = in - normal * backoff;
}
