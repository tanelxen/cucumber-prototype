//
//  SpriteRenderer.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 24.09.26.
//

#pragma once

#include <glm/glm.hpp>
#include <string>

namespace SpriteRenderer {
    
    enum class BlendMode {
        Alpha,    // Для дыма, крови, пыли (стандартная прозрачность)
        Additive  // Для искр, вспышек, лазеров (свечение)
    };
    
    // Вызывается один раз при старте движка после инициализации OpenGL
    void Init();
    
    // Освобождает текстуры и буферы при выходе
    void Shutdown();
    
    // Загружает текстуру и возвращает её OpenGL ID
    int PreloadTexture(const std::string& filename);
    
    // Добавляет биллборд в очередь отрисовки на один текущий кадр
    void AddBillboard(int textureId, glm::vec3 position, float scale, float zRot,
                      const glm::vec4& color = glm::vec4(1.0f), BlendMode blendMode = BlendMode::Alpha);
    
    // Для трасиров
    void AddAxialBillboard(int textureId, glm::vec3 position, float length, glm::vec3 direction, float width,
                           const glm::vec4& color = glm::vec4(1.0f), BlendMode blendMode = BlendMode::Alpha);
    
    // Отрисовываем все накопленные батчи и очищаем очередь
    void Flush(glm::mat4 view, glm::mat4 projection);
    void Clear();
}
