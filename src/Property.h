//
//  Property.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 03.10.26.
//

#pragma once

template<typename T>
class Property
{
private:
    T m_value;       // Текущее значение
    T m_prefabValue; // Дефолтное значение из префаба
    
public:
    Property() : m_value(T()), m_prefabValue(T()) {}
    Property(const T& defaultValue) : m_value(defaultValue), m_prefabValue(defaultValue) {}
    
    const T& get() const { return m_value; }
    
    void set(const T& newValue) { m_value = newValue; }
    
    // Вызывается движком при первой инициализации префаба
    void setFromPrefab(const T& prefabValue) {
        m_prefabValue = prefabValue;
        m_value = prefabValue;
    }
    
    // Свойство изменено, только если оно реально отличается от префаба
    bool isOverridden() const { return m_value != m_prefabValue; }
    
    void revert() { m_value = m_prefabValue; }
    
    operator const T&() const { return m_value; }
    Property& operator=(const T& newValue) { set(newValue); return *this; }
};
