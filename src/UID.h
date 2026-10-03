//
//  UID.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 11.09.26.
//

#pragma once

#include <string>
#include "../vendor/json.hpp"

struct UID
{
    uint64_t m_value = 0;
    
    UID();
    UID(const std::string& text);
    UID(uint64_t value) : m_value(value) {}
    
    static UID Invalid() { return UID(0); }
    bool isValid() const { return m_value != 0; }
    
    bool operator==(const UID& other) const { return m_value == other.m_value; }
    bool operator!=(const UID& other) const { return m_value != other.m_value; }
    
    std::string toText() const;
};

// --- Интеграция с библиотекой nlohmann/json ---
// Названия функций to_json и from_json обязаны быть именно такими и находиться в той же области видимости,
// что и структура UUID (или в ADL-видимых пространствах имен). Тогда библиотека подхватит их автоматически.

inline void to_json(nlohmann::json& j, const UID& uuid) {
    j = uuid.toText(); // В JSON всегда сохраняем в виде красивой строки
}

inline void from_json(const nlohmann::json& j, UID& uuid) {
    if (j.is_string()) {
        uuid = UID(j.get<std::string>());
    } else if (j.is_number_integer()) {
        // Защита на случай, если в JSON попадет чистое число
        uuid.m_value = j.get<uint64_t>();
    } else {
        uuid = UID::Invalid();
    }
}

// --- Поддержка контейнеров std::unordered_map и std::unordered_set ---
namespace std {
    template<>
    struct hash<UID> {
        std::size_t operator()(const UID& uuid) const noexcept {
            return std::hash<uint64_t>{}(uuid.m_value); // Хэширование происходит мгновенно
        }
    };
}
