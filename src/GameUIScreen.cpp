//
//  GameUIScreen.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 30.09.26.
//

#include "GameUIScreen.h"
#include "ProjectManager.h"

#include <fstream>
#include <iostream>

#include "../vendor/json.hpp"

using json = nlohmann::json;

namespace GameUI {
    
    void Screen::Init(const std::string& filename)
    {
        auto path = ProjectManager::Get().assetsPath() / filename;
        
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

namespace GameUI {
    
    void Screen::Save(const std::string& path) {
        json data;
        data["widgets"] = json::array();
        
        for (const auto& w : m_widgets) {
            json item;
            // Запись типа
            if (w.type == WidgetType::Rect) item["type"] = "rect";
            else if (w.type == WidgetType::Image) item["type"] = "image";
            else if (w.type == WidgetType::Text) item["type"] = "text";
            
            // Если вы добавили поле name в WidgetDefinition: item["name"] = w.name;
            item["text"] = w.text;
            item["textureId"] = w.textureId;
            item["color"] = { w.color.x, w.color.y, w.color.z, w.color.w };
            
            json lay;
            lay["anchorMinX"] = w.layout.anchorMin.x;
            lay["anchorMinY"] = w.layout.anchorMin.y;
            lay["anchorMaxX"] = w.layout.anchorMax.x;
            lay["anchorMaxY"] = w.layout.anchorMax.y;
            lay["left"] = w.layout.marginLeft;
            lay["top"] = w.layout.marginTop;
            lay["right"] = w.layout.marginRight;
            lay["bottom"] = w.layout.marginBottom;
            lay["pivotX"] = w.layout.pivot.x;
            lay["pivotY"] = w.layout.pivot.y;
            
            item["layout"] = lay;
            data["widgets"].push_back(item);
        }
        
        std::ofstream file(path);
        if (file.is_open()) {
            file << data.dump(4); // Запись с красивыми отступами
        }
    }

    // Методы для манипуляции списком элементов в редакторе
    void Screen::AddWidget(const std::string& name, WidgetType type)
    {
        WidgetDefinition def;
        def.type = type;
        def.layout = Layout::Center(100, 100); // Пресет по умолчанию
        m_widgets.push_back(def);
        m_nameToIndex[name] = m_widgets.size() - 1;
    }
    
    void Screen::RemoveWidget(size_t index)
    {
        if (index >= m_widgets.size()) return;
        m_widgets.erase(m_widgets.begin() + index);
        
        // Перестраиваем мапу индексов
        m_nameToIndex.clear();
        for (size_t i = 0; i < m_widgets.size(); ++i) {
            if (!m_widgets[i].text.empty() || m_widgets[i].textureId != 0) { // или если у вас хранится w.name
                // Рекомендуется добавить явное поле std::string name внутрь WidgetDefinition для удобства редактора
            }
        }
    }
}
