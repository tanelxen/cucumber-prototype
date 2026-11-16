//
//  LightmapCache.cpp
//  Cucumber
//

#include "LightmapCache.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <system_error>

namespace
{
    constexpr char kMagic[4] = { 'C', 'L', 'M', 'P' };
    constexpr uint32_t kFormatVersion = 1;
    constexpr int kMaxSize = 32768;

    constexpr uint64_t kSeed = 1469598103934665603ull;
    constexpr uint64_t kPrime = 1099511628211ull;

    // FNV-1a по 8 байт за шаг: достаточно быстро для нескольких МБ геометрии
    uint64_t HashBytes(uint64_t h, const void* data, size_t size)
    {
        const auto* bytes = static_cast<const uint8_t*>(data);

        while (size >= 8)
        {
            uint64_t word;
            std::memcpy(&word, bytes, sizeof(word));
            h = (h ^ word) * kPrime;
            bytes += 8;
            size -= 8;
        }
        while (size > 0)
        {
            h = (h ^ *bytes) * kPrime;
            ++bytes;
            --size;
        }
        return h;
    }

    template <typename T>
    uint64_t HashValue(uint64_t h, const T& value)
    {
        return HashBytes(h, &value, sizeof(T));
    }

    template <typename T>
    void WriteValue(std::ofstream& stream, const T& value)
    {
        stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
    }

    template <typename T>
    bool ReadValue(std::ifstream& stream, T& value)
    {
        stream.read(reinterpret_cast<char*>(&value), sizeof(T));
        return static_cast<bool>(stream);
    }
}

uint64_t LightmapCache::geometryHash(const std::vector<VertexPosition>& positions,
                                     const std::vector<glm::vec2>& lightmapTexCoords,
                                     const std::vector<uint32_t>& indices,
                                     int atlasSize)
{
    uint64_t h = kSeed;
    h = HashValue(h, atlasSize);
    h = HashValue(h, positions.size());
    h = HashValue(h, indices.size());
    h = HashBytes(h, positions.data(), positions.size() * sizeof(VertexPosition));
    h = HashBytes(h, lightmapTexCoords.data(), lightmapTexCoords.size() * sizeof(glm::vec2));
    h = HashBytes(h, indices.data(), indices.size() * sizeof(uint32_t));
    return h;
}

uint64_t LightmapCache::lightsHash(const std::vector<LightmapBakeLight>& lights,
                                   const LightmapBakeSettings& settings)
{
    uint64_t h = kSeed;
    h = HashValue(h, kLightmapBakerVersion);
    h = HashValue(h, settings.dilationPasses);
    h = HashValue(h, settings.rayBias);
    h = HashValue(h, settings.ambient);
    h = HashValue(h, lights.size());

    for (const auto& light : lights)
    {
        h = HashValue(h, static_cast<int>(light.type));
        h = HashValue(h, light.position);
        h = HashValue(h, light.direction);
        h = HashValue(h, light.color);
        h = HashValue(h, light.intensity);
        h = HashValue(h, light.range);
        h = HashValue(h, light.spotAngle);
    }
    return h;
}

bool LightmapCache::save(const std::filesystem::path& path, const Entry& entry, std::string& error)
{
    const size_t expected = static_cast<size_t>(entry.width) * static_cast<size_t>(entry.height) * 3;
    if (entry.width <= 0 || entry.height <= 0 || entry.pixels.size() != expected)
    {
        error = "Lightmap cache: pixel data does not match its size";
        return false;
    }

    std::filesystem::path temporary = path;
    temporary += ".tmp";

    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = "Lightmap cache: cannot open " + temporary.string();
            return false;
        }

        stream.write(kMagic, sizeof(kMagic));
        WriteValue(stream, kFormatVersion);
        WriteValue(stream, entry.geometryHash);
        WriteValue(stream, entry.lightsHash);
        WriteValue(stream, static_cast<int32_t>(entry.width));
        WriteValue(stream, static_cast<int32_t>(entry.height));
        stream.write(reinterpret_cast<const char*>(entry.pixels.data()),
                     static_cast<std::streamsize>(entry.pixels.size() * sizeof(uint16_t)));

        if (!stream)
        {
            error = "Lightmap cache: write failed";
            return false;
        }
    }

    std::error_code code;
    std::filesystem::rename(temporary, path, code);
    if (code)
    {
        // На некоторых платформах rename не перезаписывает существующий файл
        std::filesystem::remove(path, code);
        std::filesystem::rename(temporary, path, code);
        if (code)
        {
            error = "Lightmap cache: cannot replace " + path.string() + ": " + code.message();
            return false;
        }
    }

    error.clear();
    return true;
}

bool LightmapCache::load(const std::filesystem::path& path, Entry& entry, std::string& error)
{
    entry = {};

    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        error = "no cache file";
        return false;
    }

    char magic[4];
    stream.read(magic, sizeof(magic));
    uint32_t version = 0;
    int32_t width = 0;
    int32_t height = 0;

    if (!stream || std::memcmp(magic, kMagic, sizeof(kMagic)) != 0
        || !ReadValue(stream, version) || version != kFormatVersion
        || !ReadValue(stream, entry.geometryHash) || !ReadValue(stream, entry.lightsHash)
        || !ReadValue(stream, width) || !ReadValue(stream, height))
    {
        entry = {};
        error = "unrecognized or outdated cache file";
        return false;
    }

    if (width <= 0 || height <= 0 || width > kMaxSize || height > kMaxSize)
    {
        entry = {};
        error = "invalid cache dimensions";
        return false;
    }

    const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height) * 3;
    entry.pixels.resize(count);
    stream.read(reinterpret_cast<char*>(entry.pixels.data()), static_cast<std::streamsize>(count * sizeof(uint16_t)));
    if (!stream)
    {
        entry = {};
        error = "cache file is truncated";
        return false;
    }

    entry.width = width;
    entry.height = height;
    error.clear();
    return true;
}
