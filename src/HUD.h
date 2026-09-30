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
    
    struct Layout {
        // Где находятся границы контейнера на экране в процентах (от 0.0 до 1.0)
        glm::vec2 anchorMin = glm::vec2(0.0f, 0.0f); // Левый верхний угол экрана по умолчанию
        glm::vec2 anchorMax = glm::vec2(0.0f, 0.0f); // Если Min == Max, то это Фиксированный размер
        
        // Отступы в пикселях от этих якорей (заменяют и offset, и size)
        float marginLeft = 0.0f;
        float marginTop = 0.0f;
        float marginRight = 0.0f;  // Если Min == Max, то это ширина
        float marginBottom = 0.0f; // Если Min == Max, то это высота
        
        // Точка опоры для вращения/масштабирования (от 0.0 до 1.0 внутри самого элемента)
        glm::vec2 pivot = glm::vec2(0.0f, 0.0f);
        
        static Layout TopLeft(float x, float y, float width, float height);
        static Layout TopRight(float x, float y, float width, float height);
        static Layout BottomLeft(float x, float y, float width, float height);
        static Layout BottomRight(float x, float y, float width, float height);
        static Layout Center(float width, float height, glm::vec2 offset = glm::vec2(0.0f));
        static Layout FullScreen(float margin = 0.0f);
    };
    
    // Инициализация (вызывать после инициализации OpenGL)
    void Init();
    
    // Обновление матриц при изменении размера окна
    void Resize(int width, int height);
    
    int GetScreenWidth();
    int GetScreenHeight();
    
    // Очистка ресурсов
    void Shutdown();
    
    void DrawRect(const Layout& layout, const glm::vec4& color);
    void DrawTexture(const Layout& layout, unsigned int texture, const glm::vec4& color = glm::vec4(1.0f));
    void DrawText(const Layout& layout, const std::string& text, const glm::vec4& color = glm::vec4(1.0f));
    
    // Отрисовка всего накопленного и очистка буферов (вызывать в конце кадра)
    void Flush();
}
