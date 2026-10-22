//
//  LightmapCache.h
//  Cucumber
//
//  Кэш запечённой лайтмапы на диске. Лежит рядом с картой (<map>.lightmap) и подгружается при загрузке карты,
//  поэтому после перезапуска редактора не нужно запекать заново.
//
//  Два независимых хэша:
//   - geometryHash: UV-раскладка и геометрия. Не совпал -> файл относится к другой карте, не используем.
//   - lightsHash:   параметры запечённых ламп и настройки бейка. Не совпал -> лайтмапа показывается,
//                   но помечается как устаревшая (свет в сцене изменился после бейка).
//

#pragma once

#include "LightmapBaker.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

class LightmapCache
{
public:
    struct Entry
    {
        uint64_t geometryHash = 0;
        uint64_t lightsHash = 0;
        int width = 0;
        int height = 0;
        std::vector<uint16_t> pixels;   // RGB half, width * height * 3
    };

    static uint64_t geometryHash(const std::vector<VertexPosition>& positions,
                                 const std::vector<glm::vec2>& lightmapTexCoords,
                                 const std::vector<uint32_t>& indices,
                                 int atlasSize);

    static uint64_t lightsHash(const std::vector<LightmapBakeLight>& lights,
                               const LightmapBakeSettings& settings);

    // Пишет через временный файл + rename, чтобы оборванная запись не оставляла битый кэш.
    static bool save(const std::filesystem::path& path, const Entry& entry, std::string& error);
    static bool load(const std::filesystem::path& path, Entry& entry, std::string& error);
};
