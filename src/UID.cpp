//
//  UID.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 11.09.26.
//

#include "UID.h"

#include <random>
#include <sstream>
#include <algorithm>

// Алфавит Base34 (34 символа, исключены неоднозначные знаки вроде 'z' и '9')
static constexpr uint8_t UUID_BASE34_CHARACTERS[] = {
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y',
    '0', '1', '2', '3', '4', '5', '6', '7', '8'
};

static constexpr uint32_t UUID_BASE34_SIZE = sizeof(UUID_BASE34_CHARACTERS);
static constexpr uint32_t UUID_MAX_STRING_LENGTH = 13;

UID::UID()
{
    static thread_local std::random_device rd;
    static thread_local std::mt19937_64 generator(rd());
    static std::uniform_int_distribution<uint64_t> distribution;
    
    // Гарантируем, что сгенерированный ID не равен 0 (0 зарезервирован под невалидный ID)
    do {
        m_value = distribution(generator);
    } while (m_value == 0);
}

std::string UID::toText() const
{
    if (m_value == 0) {
        return "uid://<invalid>";
    }
    
    char32_t tmp[UUID_MAX_STRING_LENGTH];
    uint32_t tmp_size = 0;
    uint64_t id_copy = m_value;
    
    do {
        uint32_t c = id_copy % UUID_BASE34_SIZE;
        tmp[tmp_size] = UUID_BASE34_CHARACTERS[c];
        id_copy /= UUID_BASE34_SIZE;
        ++tmp_size;
    } while (id_copy);
    
    std::string txt;
    txt.resize(tmp_size + 6);
    
    txt[0] = 'u'; txt[1] = 'i'; txt[2] = 'd';
    txt[3] = ':'; txt[4] = '/'; txt[5] = '/';
    
    uint32_t size = 6;
    for (uint32_t i = 0; i < tmp_size; ++i) {
        txt[size++] = tmp[tmp_size - i - 1];
    }
    
    return txt;
}

UID::UID(const std::string& text)
{
    m_value = 0;
    
    if (text.length() <= 6 || !text.starts_with("uid://") || text.starts_with("uid://<invalid")) {
        return;
    }
    
    uint64_t uid = 0;
    for (uint32_t i = 6; i < text.length(); i++) {
        uint32_t c = text[i];
        
        const uint8_t* slot = std::find(std::begin(UUID_BASE34_CHARACTERS), std::end(UUID_BASE34_CHARACTERS), c);
        if (slot == std::end(UUID_BASE34_CHARACTERS)) {
            return UID::Invalid(); // Найден невалидный символ в строке
        }
        
        uint32_t index = std::distance(std::begin(UUID_BASE34_CHARACTERS), slot);
        uid *= UUID_BASE34_SIZE;
        uid += index;
    }
    
    m_value = uid;
}
