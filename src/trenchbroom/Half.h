//
//  Half.h
//  Cucumber
//
//  float <-> IEEE 754 half. Лайтмапа хранится как RGB16F: GL читает её напрямую (GL_HALF_FLOAT),
//  а на диске она вдвое меньше float32.
//

#pragma once

#include <cstdint>
#include <cstring>

inline uint16_t FloatToHalf(float value)
{
    uint32_t x;
    std::memcpy(&x, &value, sizeof(x));

    const uint32_t sign = (x >> 16) & 0x8000u;
    const uint32_t exponent32 = (x >> 23) & 0xFFu;
    uint32_t mantissa = x & 0x7FFFFFu;

    if (exponent32 == 0xFFu) // inf / nan
        return static_cast<uint16_t>(sign | 0x7C00u | (mantissa ? 0x200u : 0u));

    const int32_t exponent = static_cast<int32_t>(exponent32) - 127 + 15;

    if (exponent >= 31) return static_cast<uint16_t>(sign | 0x7C00u);   // переполнение -> inf

    if (exponent <= 0)                                                  // субнормальные и ноль
    {
        if (exponent < -10) return static_cast<uint16_t>(sign);
        mantissa |= 0x800000u;
        const uint32_t shift = static_cast<uint32_t>(14 - exponent);
        uint32_t half = mantissa >> shift;
        if ((mantissa >> (shift - 1)) & 1u) half += 1;
        return static_cast<uint16_t>(sign | half);
    }

    uint32_t half = sign | (static_cast<uint32_t>(exponent) << 10) | (mantissa >> 13);
    if (mantissa & 0x1000u) half += 1;   // округление к ближайшему; перенос в экспоненту корректен
    return static_cast<uint16_t>(half);
}

inline float HalfToFloat(uint16_t h)
{
    const uint32_t sign = (static_cast<uint32_t>(h) & 0x8000u) << 16;
    uint32_t exponent = (h >> 10) & 0x1Fu;
    uint32_t mantissa = h & 0x3FFu;

    uint32_t bits;
    if (exponent == 0)
    {
        if (mantissa == 0)
        {
            bits = sign;
        }
        else // субнормальное: нормализуем
        {
            exponent = 127 - 15 + 1;
            while ((mantissa & 0x400u) == 0) { mantissa <<= 1; --exponent; }
            mantissa &= 0x3FFu;
            bits = sign | (exponent << 23) | (mantissa << 13);
        }
    }
    else if (exponent == 0x1Fu)
    {
        bits = sign | 0x7F800000u | (mantissa << 13);
    }
    else
    {
        bits = sign | ((exponent - 15 + 127) << 23) | (mantissa << 13);
    }

    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}
