//
//  LevelFiles.h
//  Cucumber
//
//  Файловая часть жизни уровня: загрузка .cmap.json с миграцией, сохранение, создание пустого уровня,
//  импорт .map (TrenchBroom). Без состояния и без знания о компоненте: пути относительные к папке ассетов
//  проекта, результат возвращается значениями, а текст ошибки уже готов для показа пользователю
//  (MapComponent кладёт его в bakeStatus).
//

#pragma once

#include <filesystem>
#include <string>

#include "level/LevelDocument.h"

namespace LevelFiles
{
    // Файл уровня (.cmap.json), а не исходная .map
    bool IsLevelFile(const std::string& path);

    // maps/e1m1.map -> maps/e1m1.cmap.json
    std::filesystem::path LevelPathFor(std::filesystem::path mapFile);

    // Свободное имя для нового уровня: maps/untitled.cmap.json, maps/untitled_1.cmap.json, ...
    std::filesystem::path NextUntitledPath();

    // Читает уровень и при необходимости мигрирует (старые версии: имена материалов приводятся к каноническим).
    // migrated = true: документ изменился относительно файла, его стоит пересохранить.
    bool Load(const std::filesystem::path& relativePath, LevelDocument& out, bool& migrated, std::string& error);

    bool Save(const std::filesystem::path& relativePath, const LevelDocument& level, std::string& error);

    // Создаёт на диске пустой уровень. Существующий файл не трогает (это ошибка).
    bool CreateEmpty(const std::filesystem::path& relativePath, std::string& error);

    // Импортирует .map в .cmap.json рядом с ней и возвращает путь уровня.
    // overwrite = false и документ уже есть: ничего не импортируется (правки редактора не затираются),
    // возвращается путь существующего документа.
    bool Import(const std::filesystem::path& mapFile, bool overwrite, std::filesystem::path& levelRelativePath,
                std::string& error);
}
