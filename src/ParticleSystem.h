//
//  ParticleSystem.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 24.09.26.
//

#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace ParticleSystem {
    
    struct Particle {
        glm::vec3 position     = glm::vec3(0.0f);
        glm::vec3 velocity     = glm::vec3(0.0f);
        glm::vec4 color          = glm::vec4(1.0f); // Для плавного затухания
        float size             = 1.0f;
        float life             = 0.0f;            // Сколько осталось жить (в секундах)
        float maxLife          = 1.0f;
        bool active            = false;
    };
    
    // Инициализация (загрузка текстур)
    void Init();
    
    // Обновление всех частиц (гравитация, скорость, время жизни)
    void Update(float dt);
    
    // Главный метод для вызова из AngelScript: спавнит фонтан искр в точке
    void SpawnSparks(const glm::vec3& position, const glm::vec3& normal, int count);
    
    void Shutdown();
}
