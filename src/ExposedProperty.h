//
//  ExposedProperty.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 04.10.26.
//

#pragma once

#include <string>
#include "../vendor/json.hpp"

struct ExposedProperty
{
    std::string displayName;   // Что видит пользователь в UI (например, "Speed")
    std::string targetNodePath;// Путь к целевому объекту внутри префаба (например, "Visuals/Sprite")
    std::string componentType; // Название компонента (например, "MovementComponent")
    std::string propertyName;  // Название переменной внутри компонента (например, "max_speed")
    
    // Сериализация метаданных экспорта (записывается ТОЛЬКО в файл .prefab)
    void save(nlohmann::json& j) const
    {
        j = {
            {"display_name", displayName},
            {"target_path", targetNodePath},
            {"component_type", componentType},
            {"property_name", propertyName}
        };
    }
    
    void load(const nlohmann::json& j)
    {
        displayName = j.at("display_name").get<std::string>();
        targetNodePath = j.at("target_path").get<std::string>();
        componentType = j.at("component_type").get<std::string>();
        propertyName = j.at("property_name").get<std::string>();
    }
};

struct CachedExposedProperty
{
    std::string displayName; // Внешнее имя (например, "Monster Speed")
    std::string propertyName;// Внутреннее имя переменной (например, "speed")
    class Component* targetComponent = nullptr; // Прямой указатель на живой компонент в памяти!
};
