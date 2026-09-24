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
    
    const int MAX_PARTICLES = 2000; // Максимальное число искр на экране
    static std::vector<Particle> s_particlePool;
    static int s_lastUsedParticle = 0;
    static int s_sparkTextureId = 0;
    
    // Вспомогательная функция рандома
    float RandomFloat(float min, float max)
    {
        return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) / (max - min));
    }
    
    void Init()
    {
        s_particlePool.resize(MAX_PARTICLES);
        // Предполагаем, что у тебя есть текстура круглой искры или яркой точки
        s_sparkTextureId = SpriteRenderer::PreloadTexture("assets/textures/tracer2.jpg");
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
    
    void SpawnSparks(const glm::vec3& position, const glm::vec3& normal, int count)
    {
        for (int i = 0; i < count; i++) {
            int idx = FindUnusedParticle();
            auto& p = s_particlePool[idx];
            
            p.position = position;
            p.active = true;
            p.maxLife = RandomFloat(0.5f, 1.0f); // Искры живут недолго (0.2 - 0.5 сек)
            p.life = p.maxLife;
            p.size = RandomFloat(4.0f, 16.0f);
            
            // Генерируем направление вылета в конусе вокруг нормали поверхности
            glm::vec3 randomDir(
                                RandomFloat(-0.6f, 0.6f),
                                RandomFloat(-0.6f, 0.6f),
                                RandomFloat(-0.6f, 0.6f)
                                );
            
            // Направление = нормаль + небольшая случайная девиация
            glm::vec3 velocityDir = glm::normalize(normal + randomDir);
            float speed = RandomFloat(1.0f * 38.0f, 4.0f * 38.0f); // Скорость вылета искр
            p.velocity = velocityDir * speed;
        }
    }
    
    void Update(float dt)
    {
        const glm::vec3 gravity(0.0f, -9.81f * 38.0f, 0.0f); // Сила притяжения для искр
        
        for (auto& p : s_particlePool) {
            if (!p.active) continue;
            
            p.life -= dt;
            if (p.life <= 0.0f) {
                p.active = false;
                continue;
            }
            
            // Физика Ньютона: скорость + гравитация, позиция + скорость
            p.velocity += gravity * dt;
            p.position += p.velocity * dt;
            
            // Вычисляем затухание (от 1.0 до 0.0 к концу жизни)
            float alpha = p.life / p.maxLife;
            
            // Отправляем частицу в SpriteRenderer на отрисовку в ЭТОМ кадре.
            // Так как это искры, мы можем дать им случайный поворот для естественности.
            float randomRot = p.life * 10.0f; // Они будут красиво вращаться в полете
            
            // Передаем в наш Batch-рендерер
            SpriteRenderer::AddBillboard(s_sparkTextureId, p.position, p.size * alpha, randomRot);
        }
    }
    
    void Shutdown()
    {
        s_particlePool.clear();
    }
}
