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

namespace GameUI {
    
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
    
    void DrawText(const Layout& layout,
                  const std::string& text,
                  const glm::vec4& color = glm::vec4(1),
                  bool drawShadow = false,
                  const glm::vec4& shadowColor = glm::vec4(0)
                  );
    
    // Отрисовка всего накопленного и очистка буферов (вызывать в конце кадра)
    void Flush();
    void Clear();
}

namespace GameUI {
    enum class WidgetType { Rect, Image, Text };
    
    struct WidgetDefinition {
        WidgetType type;
        Layout layout;
        glm::vec4 color = glm::vec4(1.0f);
        
        bool hasShadow = true;
        glm::vec4 shadowColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        
        // Специфичные данные для типов
        unsigned int textureId = 0;
        std::string text = "";
        
        WidgetDefinition& TopLeft(float x, float y, float w, float h) {
            layout = Layout::TopLeft(x, y, w, h);
            return *this;
        }
        
        WidgetDefinition& TopRight(float x, float y, float w, float h) {
            layout = Layout::TopRight(x, y, w, h);
            return *this;
        }
        
        WidgetDefinition& BottomLeft(float x, float y, float w, float h) {
            layout = Layout::BottomLeft(x, y, w, h);
            return *this;
        }
        
        WidgetDefinition& BottomRight(float x, float y, float w, float h) {
            layout = Layout::BottomRight(x, y, w, h);
            return *this;
        }
        
        WidgetDefinition& Center(float w, float h, glm::vec2 offset = glm::vec2(0.0f)) {
            layout = Layout::Center(w, h, offset);
            return *this;
        }
        
        WidgetDefinition& FullScreen() {
            layout = Layout::FullScreen();
            return *this;
        }
        
        WidgetDefinition& SetColor(const glm::vec4& c) {
            color = c;
            return *this;
        }
        
        WidgetDefinition& SetShadow(bool enable, const glm::vec4& sColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.75f)) {
            hasShadow = enable;
            shadowColor = sColor;
            return *this;
        }
        
        WidgetDefinition& SetPivot(const glm::vec2& p) {
            layout.pivot = p;
            return *this;
        }
        
        // Финальный вызов, который отправляет геометрию в батчер HUD
        void Draw() {
            if (type == WidgetType::Rect) {
                GameUI::DrawRect(layout, color);
            } else if (type == WidgetType::Image) {
                GameUI::DrawTexture(layout, textureId, color);
            } else if (type == WidgetType::Text) {
                GameUI::DrawText(layout, text, color, hasShadow, shadowColor);
            }
        }
    };
    
    // Фабрики в самом HUD, которые создают начальный заготовленный строитель
    inline WidgetDefinition Rect(const glm::vec4& color) {
        WidgetDefinition def;
        def.type = WidgetType::Rect;
        def.color = color;
        return def;
    }
    
    inline WidgetDefinition Image(unsigned int textureId) {
        WidgetDefinition def;
        def.type = WidgetType::Image;
        def.textureId = textureId;
        return def;
    }
    
    inline WidgetDefinition Text(const std::string& text, const glm::vec4& color = glm::vec4(1.0f)) {
        WidgetDefinition def;
        def.type = WidgetType::Text;
        def.text = text;
        def.color = color;
        return def;
    }
}
