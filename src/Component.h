//
//  Component.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 10.09.26.
//

#pragma once

#include "Property.h"

#include "../vendor/json.hpp"
#include <typeindex>

class Component
{
public:
    enum Flags : uint8_t {
        None = 0,
        RequiresUpdate = 1 << 0,
        RequiresFixedUpdate = 1 << 1
    };
    
    bool enabled = true;
    bool isPendingKill = false;
    class Entity* owner = nullptr;
    
    virtual ~Component() = default;
    
    virtual void awake() {}
    virtual void start() {}
    virtual void update(float dt) {}
    
    virtual void gizmo() {}
    
    // Полное сохранение (для файлов префабов и обычных сущностей сцены)
    virtual void save(nlohmann::json& outData) const = 0;
    
    // Сохранение ТОЛЬКО измененных полей (для оверрайдов префаба на сцене)
    virtual void saveOverrides(nlohmann::json& outData) const { };
    
    virtual void load(const nlohmann::json& inData) = 0;
    
    virtual const std::string getTypeName() const = 0;
    
    virtual std::type_index GetTypeIndex() const {
        return std::type_index(typeid(*this));
    }
    
    bool HasFlag(Flags flag) const { return (m_flags & flag) != 0; }
    
protected:
    uint8_t m_flags = None;
    
    // Конструктор по умолчанию для "обычных" компонентов (Transform, Mesh)
    Component() = default;
    
    // Конструктор с флагами для продвинутых компонентов
    Component(uint8_t flags) : m_flags(flags) {}
};
