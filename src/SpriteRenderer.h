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
    // Вызывается один раз при старте движка после инициализации OpenGL
    void Init();
    
    // Освобождает текстуры и буферы при выходе
    void Shutdown();
    
    // Загружает текстуру и возвращает её OpenGL ID
    int PreloadTexture(const std::string& filename);
    
    // Добавляет биллборд в очередь отрисовки на один текущий кадр
    void AddBillboard(int textureId, glm::vec3 position, float scale, float zRot);
    
    // Отрисовывает все накопленные батчи и очищает очередь
    void Flush(glm::mat4 view, glm::mat4 projection);
}
