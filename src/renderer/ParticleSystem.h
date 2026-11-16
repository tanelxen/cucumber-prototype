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
    
    // Инициализация (загрузка текстур)
    void Init();
    
    // Обновление всех частиц (гравитация, скорость, время жизни)
    void Update(float dt);
    
    void SpawnSparks(const glm::vec3& position, const glm::vec3& normal, int count);
    void SpawnBlood(const glm::vec3& position, const glm::vec3& normal, int count);
    void SpawnDust(const glm::vec3& position, int count);
    void SpawnSmoke(const glm::vec3& position, int count);
    void SpawnTracer(const glm::vec3& start, const glm::vec3& end);
    
    void Shutdown();
}
