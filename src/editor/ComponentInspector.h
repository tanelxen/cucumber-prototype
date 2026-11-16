//
//  InspectorComponents.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 13.10.26.
//

#pragma once

#include <string>
#include <typeindex>
#include <functional>
#include <unordered_map>

class Component;
class InspectorPanel;

class ComponentInspector
{
public:
    // Сигнатура функции отрисовки: принимает чистый указатель на компонент
    using RenderFunc = std::function<void(Component*, InspectorPanel* p)>;
    
    static ComponentInspector& Get() {
        static ComponentInspector instance;
        return instance;
    }
    
    // Регистрация нового UI для компонента
    template<typename T>
    void Register(const std::string& displayName, RenderFunc func) {
        std::type_index id = std::type_index(typeid(T));
        registry[id] = { displayName, func };
    }
    
    // Отрисовка компонента по его динамическому типу
    void Render(Component* component, InspectorPanel* panel);
    
    // Дополнительный метод для получения красивого имени (для меню добавления)
    std::string GetName(std::type_index id);
    
private:
    struct InspectorInfo {
        std::string displayName;
        RenderFunc renderFunc;
    };
    
    std::unordered_map<std::type_index, InspectorInfo> registry;
};

void RegisterComponentsUI();
