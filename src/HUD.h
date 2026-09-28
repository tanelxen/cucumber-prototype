//
//  HUD.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 28.09.26.
//

#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace HUD {
    
    // Инициализация (вызывать после инициализации OpenGL)
    void Init();
    
    // Обновление матриц при изменении размера окна
    void Resize(int width, int height);
    
    int GetScreenWidth();
    int GetScreenHeight();
    
    // Очистка ресурсов
    void Shutdown();
    
    // Начало сбора команд (вызывать в начале кадра)
    void Begin();
    
    // Функции добавления геометрии в батч (Draw)
    void DrawRect(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color);
    void DrawTexture(const glm::vec2& pos, const glm::vec2& size, unsigned int textureID, const glm::vec4& color = glm::vec4(1.0f));
    void DrawText(const std::string& text, const glm::vec2& pos, float scale, const glm::vec4& color);
    
    // Отрисовка всего накопленного и очистка буферов (вызывать в конце кадра)
    void Flush();
}
