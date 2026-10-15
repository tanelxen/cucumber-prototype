//
//  GameUIScreen.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 30.09.26.
//

#pragma once

#include "GameUI.h"

#include <string>
#include <vector>
#include <unordered_map>

namespace GameUI {
    
    class Screen
    {
    public:
        Screen() = default;
        ~Screen() = default;
        
        // Управление временем жизни для AngelScript (Reference Counting)
        void AddRef() { m_refCount++; }
        void Release() { if (--m_refCount == 0) delete this; }
        
        // Загрузка экрана из JSON файла
        void Init(const std::string& filename);
        void Save(const std::string& filename);
        
        void Draw();
        
        // Динамическое изменение свойств элементов из кода/скриптов
        void SetTexture(const std::string& name, unsigned int textureId);
        void SetText(const std::string& name, const std::string& text);
        void SetColor(const std::string& name, const glm::vec4& color);
        
        // Получить прямой ID (индекс в массиве) виджета по его имени
        int GetWidgetId(const std::string& name) const;
        void SetTexture(int id, unsigned int textureId);
        void SetText(int id, const std::string& text);
        void SetColor(int id, const glm::vec4& color);
        
    public:
        void AddWidget(const std::string& name, WidgetType type);
        void RemoveWidget(size_t index);
        
        // Доступ к вектору виджетов для инспектора и иерархии
        std::vector<WidgetDefinition>& GetWidgets() { return m_widgets; }
        
    private:
        // Храним сырые заготовки виджетов, которые мы прочитали из JSON
        std::vector<WidgetDefinition> m_widgets;
        // Быстрый поиск индекса виджета в векторе по его текстовому ID (имени)
        std::unordered_map<std::string, size_t> m_nameToIndex;
        
        // Счетчик ссылок для AngelScript
        int m_refCount = 1;
    };
}
