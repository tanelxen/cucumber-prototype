//
//  ParticleSystem.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 24.09.26.
//

#include "ParticleSystem.h"
#include "SpriteRenderer.h"
#include <cstdlib> // для rand()

namespace ParticleSystem {
    
    enum class ParticleType {
        Spark,
        Blood,
        Dust,
        Smoke,
        Tracer
    };
    
    struct Particle {
        glm::vec3 position     = glm::vec3(0.0f);
        glm::vec3 velocity     = glm::vec3(0.0f);
        glm::vec4 color          = glm::vec4(1.0f); // Для плавного затухания
        float size             = 1.0f;
        float life             = 0.0f;            // Сколько осталось жить (в секундах)
        float maxLife          = 1.0f;
        ParticleType type;                      // ТИП ЧАСТИЦЫ
        glm::vec3 direction;                    // Для трасеров
        bool active            = false;
    };
    
    const int MAX_PARTICLES = 2000; // Максимальное число искр на экране
    static std::vector<Particle> s_particlePool;
    static int s_lastUsedParticle = 0;
//    static int s_sparkTextureId = 0;
    
    // Текстуры для разных эффектов
    static int s_texSpark = 0;
    static int s_texBlood = 0;
    static int s_texDust = 0;
    static int s_texSmoke = 0;
    static int s_texTracer = 0;
    
    // Вспомогательная функция рандома
    float RandomFloat(float min, float max)
    {
        return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) / (max - min));
    }
    
    void Init()
    {
        s_particlePool.resize(MAX_PARTICLES);

        s_texSpark  = SpriteRenderer::PreloadTexture("assets/textures/tracer2.jpg");
        s_texBlood  = SpriteRenderer::PreloadTexture("assets/textures/blood_dot1.png");
        s_texDust   = SpriteRenderer::PreloadTexture("assets/textures/smokepuff.png");
        s_texSmoke  = SpriteRenderer::PreloadTexture("assets/textures/smokepuff_b1.png");
        s_texTracer = SpriteRenderer::PreloadTexture("assets/textures/tracer2.jpg");
    }
    
    // Поиск мертвой частицы в пуле (работает за O(1) в большинстве случаев)
    int FindUnusedParticle()
    {
        for (int i = s_lastUsedParticle; i < MAX_PARTICLES; i++) {
            if (!s_particlePool[i].active) {
                s_lastUsedParticle = i;
                return i;
            }
        }
        for (int i = 0; i < s_lastUsedParticle; i++) {
            if (!s_particlePool[i].active) {
                s_lastUsedParticle = i;
                return i;
            }
        }
        return 0; // Перезаписываем первую, если пул забит целиком
    }
    

    
//    void Update(float dt)
//    {
//        const glm::vec3 gravity(0.0f, -9.81f * 38.0f, 0.0f); // Сила притяжения для искр
//        
//        for (auto& p : s_particlePool) {
//            if (!p.active) continue;
//            
//            p.life -= dt;
//            if (p.life <= 0.0f) {
//                p.active = false;
//                continue;
//            }
//            
//            // Физика Ньютона: скорость + гравитация, позиция + скорость
//            p.velocity += gravity * dt;
//            p.position += p.velocity * dt;
//            
//            // Вычисляем затухание (от 1.0 до 0.0 к концу жизни)
//            float alpha = p.life / p.maxLife;
//            
//            // Отправляем частицу в SpriteRenderer на отрисовку в ЭТОМ кадре.
//            // Так как это искры, мы можем дать им случайный поворот для естественности.
//            float randomRot = p.life * 10.0f; // Они будут красиво вращаться в полете
//            
//            // Передаем в наш Batch-рендерер
//            SpriteRenderer::AddBillboard(s_sparkTextureId, p.position, p.size * alpha, randomRot);
//        }
//    }
    
    void Shutdown()
    {
        s_particlePool.clear();
    }
}

void ParticleSystem::SpawnSparks(const glm::vec3& position, const glm::vec3& normal, int count)
{
    for (int i = 0; i < count; i++)
    {
        int idx = FindUnusedParticle();
        auto& p = s_particlePool[idx];
        
        p.type = ParticleType::Spark;
        p.position = position;
        p.active = true;
        p.maxLife = RandomFloat(0.5f, 1.0f); // Искры живут недолго (0.2 - 0.5 сек)
        p.life = p.maxLife;
        p.size = RandomFloat(0.05f, 0.15f) * 80.f;
        
        // Генерируем направление вылета в конусе вокруг нормали поверхности
        glm::vec3 randomDir(RandomFloat(-0.6f, 0.6f),
                            RandomFloat(-0.6f, 0.6f),
                            RandomFloat(-0.6f, 0.6f));
        
        // Направление = нормаль + небольшая случайная девиация
        glm::vec3 velocityDir = glm::normalize(normal + randomDir);
        float speed = RandomFloat(1.0f, 4.0f) * 38.0f; // Скорость вылета искр
        p.velocity = velocityDir * speed;
    }
}

// КРОВЬ: Летит конусом от ранения, падает тяжело
void ParticleSystem::SpawnBlood(const glm::vec3& position, const glm::vec3& normal, int count)
{
    for (int i = 0; i < count; i++)
    {
        int idx = FindUnusedParticle();
        auto& p = s_particlePool[idx];
        
        p.type = ParticleType::Blood;
        p.active = true;
        p.position = position;
        p.maxLife = RandomFloat(0.4f, 0.8f);
        p.life = p.maxLife;
        p.size = RandomFloat(0.08f, 0.18f) * 80.f;
        
        glm::vec3 dir = glm::normalize(normal + glm::vec3(RandomFloat(-0.5f, 0.5f), RandomFloat(-0.5f, 0.5f), RandomFloat(-0.5f, 0.5f)));
        p.velocity = dir * RandomFloat(2.0f, 4.0f) * 38.0f;
    }
}

// ПЫЛЬ: Спавнится при попадании в стены, почти нет скорости, медленно кружится
void ParticleSystem::SpawnDust(const glm::vec3& position, int count)
{
    for (int i = 0; i < count; i++)
    {
        int idx = FindUnusedParticle();
        auto& p = s_particlePool[idx];
        
        p.type = ParticleType::Dust;
        p.active = true;
        p.position = position + glm::vec3(RandomFloat(-0.1f, 0.1f), RandomFloat(-0.1f, 0.1f), RandomFloat(-0.1f, 0.1f)) * 38.f;
        p.maxLife = RandomFloat(2.0f, 3.0f);
        p.life = p.maxLife; // Пыль живет долго
        p.size = RandomFloat(0.2f, 0.5f) * 96.f;
        p.velocity = glm::vec3(RandomFloat(-0.2f, 0.2f), RandomFloat(0.1f, 0.4f), RandomFloat(-0.2f, 0.2f)) * 38.f;
    }
}

// ДЫМ: Медленно поднимается вверх, расширяясь в размерах
void ParticleSystem::SpawnSmoke(const glm::vec3& position, int count)
{
    for (int i = 0; i < count; i++) {
        int idx = FindUnusedParticle();
        auto& p = s_particlePool[idx];
        p.active = true; p.type = ParticleType::Smoke;
        p.position = position;
        p.maxLife = RandomFloat(0.8f, 1.5f);
        p.life = p.maxLife;
        p.size = RandomFloat(0.1f, 0.2f) * 80.f; // Стартует маленьким
        p.velocity = glm::vec3(RandomFloat(-0.3f, 0.3f) * 38.0f, RandomFloat(0.8f, 1.5f) * 38.0f, RandomFloat(-0.3f, 0.3f) * 38.0f); // Вверх!
    }
}

// ТРАСЕР: Одна летящая линия
void ParticleSystem::SpawnTracer(const glm::vec3& start, const glm::vec3& end)
{
    int idx = FindUnusedParticle();
    auto& p = s_particlePool[idx];
    p.active = true; p.type = ParticleType::Tracer;
    p.position = start;
    p.direction = glm::normalize(end - start);
    
    float distance = glm::distance(start, end);
    float speed = 128.0f * 38.0f; // Огромная скорость полета трасера
    p.velocity = p.direction * speed;
    
    p.maxLife = distance / speed; // Умрет ровно при долете до точки попадания
    p.life = p.maxLife;
    p.size = 0.05f * 80.f; // Ширина трасера
}

void ParticleSystem::Update(float dt)
{
    const glm::vec3 gravity(0.0f, -9.81f * 38.0f, 0.0f);
    
    for (auto& p : s_particlePool)
    {
        if (!p.active) continue;
        
        p.life -= dt;
        if (p.life <= 0.0f) { p.active = false; continue; }
        
        float alpha = p.life / p.maxLife;
        
        glm::vec4 color = {1, 1, 1, 1};
        
        switch (p.type)
        {
            case ParticleType::Spark:
                p.velocity += gravity * dt;
                p.position += p.velocity * dt;
                SpriteRenderer::AddBillboard(s_texSpark, p.position, p.size, p.life * 5.0f, color, SpriteRenderer::BlendMode::Additive);
                break;
                
            case ParticleType::Blood:
                p.velocity += gravity * 1.5f * dt; // Кровь падает быстрее/тяжелее
                p.position += p.velocity * dt;
                color = { 0.5f, 0.0f, 0.0f, glm::clamp(alpha * 2.0f, 0.0f, 1.0f) };
                SpriteRenderer::AddBillboard(s_texBlood, p.position, p.size, 0.0f, color, SpriteRenderer::BlendMode::Alpha);
                break;
                
            case ParticleType::Dust:
                p.position += p.velocity * dt; // Пыль просто медленно плывет
                color = { 0.7f, 0.65f, 0.6f, alpha * 0.8f };
                SpriteRenderer::AddBillboard(s_texDust, p.position, p.size, p.life * 0.5f, color, SpriteRenderer::BlendMode::Alpha);
                break;
                
            case ParticleType::Smoke:
                p.position += p.velocity * dt;
                // Дым расширяется со временем! Стартовый размер умножаем на инвертированный коэффициент жизни
//                float smokeScale = p.size * (2.0f - alpha);
                color = { 1, 1, 1, alpha * 0.5f };
                SpriteRenderer::AddBillboard(s_texSmoke, p.position, p.size * (2.0f - alpha), p.life * 0.2f, color, SpriteRenderer::BlendMode::Alpha);
                break;
                
            case ParticleType::Tracer:
                p.position += p.velocity * dt;
                // ТРАСЕР: Желто-белая светящаяся линия, блендинг ADDITIVE
                color = { 1.0f, 0.9f, 0.6f, 0.5f };
                SpriteRenderer::AddAxialBillboard(s_texTracer, p.position, 5.f * 38, p.direction, p.size, color, SpriteRenderer::BlendMode::Additive);
                break;
        }
    }
}
