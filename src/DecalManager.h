//
//  DecalManager.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 24.09.26.
//

#pragma once
#include <glm/glm.hpp>

namespace DecalManager
{
    void Init();
    void Update(float dt); // Обновляет время жизни декалей
    void Shutdown();
    
    // Главная функция для вызова из скрипта AngelScript
    void AddDecal(int textureId, glm::vec3 position, glm::vec3 normal, float size, float lifespan);
    
    // Вызывается в основном цикле рендеринга C++
    void Flush(glm::mat4 view, glm::mat4 projection);
}
