//
//  LevelDocumentIO.h
//  Cucumber
//
//  Чтение и запись .cmap.json.
//
//  Формат записи рассчитан на diff: шапка с отступами, а каждый солид ровно в одной строке.
//  Числа пишутся в кратчайшем виде, читающемся обратно бит в бит: документ в памяти и на диске идентичны,
//  "сохранить -> загрузить -> сохранить" даёт тот же файл, а результат кукера (и хэш лайтмапы) не зависит от того,
//  из памяти он собран или из файла.
//

#pragma once

#include "LevelDocument.h"

#include <filesystem>
#include <string>

namespace LevelDocumentIO
{
    bool Load(const std::filesystem::path& path, LevelDocument& out, std::string& error);
    bool Save(const std::filesystem::path& path, const LevelDocument& document, std::string& error);

    // То же без файла (отладка, тесты, буфер обмена)
    bool FromString(const std::string& text, LevelDocument& out, std::string& error);
    bool ToString(const LevelDocument& document, std::string& out, std::string& error);
}
