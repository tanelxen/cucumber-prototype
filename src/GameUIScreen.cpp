//
//  GameUIScreen.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 30.09.26.
//

#include "GameUIScreen.h"

#include <fstream>
#include <iostream>

#include "../vendor/json.hpp"

using json = nlohmann::json;

namespace GameUI {
    
    void Screen::Init(const std::string& path)
    {
        m_widgets.clear();
        m_nameToIndex.clear();
        
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "[HUD::Screen] Не удалось открыть JSON файл UI: " << path << std::endl;
            return;
        }
        
        json data;
        
        try {
            file >> data;
        } catch (json::parse_error& e) {
            std::cerr << "[HUD::Screen] Ошибка парсинга JSON: " << e.what() << std::endl;
            return;
        }
        
        if (!data.contains("widgets") || !data["widgets"].is_array()) return;
        
        for (auto& item : data["widgets"])
        {
            WidgetDefinition def;
            
            // Читаем тип виджета
            std::string typeStr = item.value("type", "rect");
            if (typeStr == "rect")       def.type = WidgetType::Rect;
            else if (typeStr == "image") def.type = WidgetType::Image;
            else if (typeStr == "text")  def.type = WidgetType::Text;
            
            // Имя (ID) для поиска из скриптов
            std::string name = item.value("name", "");
            
            // Базовые данные виджета
            def.text = item.value("text", "");
            def.textureId = item.value("textureId", 0); // По умолчанию 0, можно задать позже через SetTexture
            
            // Читаем цвет [RGBA]
            if (item.contains("color") && item["color"].is_array() && item["color"].size() == 4) {
                def.color = glm::vec4(item["color"][0], item["color"][1], item["color"][2], item["color"][3]);
            }
            
            // Читаем геометрию (Layout)
            if (item.contains("layout"))
            {
                auto& lay = item["layout"];
                
                // Якоря (по умолчанию 0.0 — левый верхний угол)
                def.layout.anchorMin.x = lay.value("anchorMinX", 0.0f);
                def.layout.anchorMin.y = lay.value("anchorMinY", 0.0f);
                def.layout.anchorMax.x = lay.value("anchorMaxX", 0.0f);
                def.layout.anchorMax.y = lay.value("anchorMaxY", 0.0f);
                
                // Отступы (Margins)
                def.layout.marginLeft   = lay.value("left", 0.0f);
                def.layout.marginTop    = lay.value("top", 0.0f);
                def.layout.marginRight  = lay.value("right", 0.0f);
                def.layout.marginBottom = lay.value("bottom", 0.0f);
                
                // Точка опоры (Pivot)
                def.layout.pivot.x = lay.value("pivotX", 0.0f);
                def.layout.pivot.y = lay.value("pivotY", 0.0f);
            }
            
            // Сохраняем виджет
            m_widgets.push_back(def);
            
            if (!name.empty()) {
                m_nameToIndex[name] = m_widgets.size() - 1;
            }
        }
    }
    
    void Screen::Draw()
    {
        // Просто пробегаемся по массиву и вызываем уже готовый метод отрисовки нашего билдера
        for (auto& widget : m_widgets)
        {
            widget.Draw();
        }
    }
    
    
    int Screen::GetWidgetId(const std::string& name) const {
        auto it = m_nameToIndex.find(name);
        if (it != m_nameToIndex.end()) {
            return static_cast<int>(it->second);
        }
        std::cerr << "[HUD] Виджет с именем " << name << " не найден!" << std::endl;
        return -1;
    }
    
    void Screen::SetTexture(const std::string& name, unsigned int textureId)
    {
        auto it = m_nameToIndex.find(name);
        if (it != m_nameToIndex.end()) {
            m_widgets[it->second].textureId = textureId;
        }
    }
    
    void Screen::SetTexture(int id, unsigned int textureId) {
        if (id >= 0 && id < static_cast<int>(m_widgets.size())) {
            m_widgets[id].textureId = textureId;
        }
    }
    
    void Screen::SetText(const std::string& name, const std::string& text)
    {
        auto it = m_nameToIndex.find(name);
        if (it != m_nameToIndex.end()) {
            m_widgets[it->second].text = text;
        }
    }
    
    void Screen::SetText(int id, const std::string& text) {
        if (id >= 0 && id < static_cast<int>(m_widgets.size())) {
            m_widgets[id].text = text;
        }
    }
    
    void Screen::SetColor(const std::string& name, const glm::vec4& color)
    {
        auto it = m_nameToIndex.find(name);
        if (it != m_nameToIndex.end()) {
            m_widgets[it->second].color = color;
        }
    }

    void Screen::SetColor(int id, const glm::vec4& color) {
        if (id >= 0 && id < static_cast<int>(m_widgets.size())) {
            m_widgets[id].color = color;
        }
    }
}
