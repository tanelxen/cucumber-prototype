//
//  TextureLibrary.h
//  Cucumber
//
//  Единственное место, где имя материала превращается в текстуру.
//
//  Каноническое имя материала: путь к картинке относительно корня assets, без расширения и со слешами '/',
//  например "wolf/textures/e_wall/brick" или "props/crate". Расширение не входит в имя (как и раньше, когда
//  имена приходили из TrenchBroom), поэтому две картинки с одним именем и разными расширениями в одной папке
//  считаются одной текстурой, как и в TextureManager.
//
//  Библиотека держит два независимых кэша:
//    - индекс файлов на диске (entries/contains): строится лениво, обновляется refresh();
//    - сведения о загруженных текстурах (info): размер и GL-хэндл, поверх TextureManager.
//
//  Только главный поток: TextureManager и GL-хэндлы не потокобезопасны.
//

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class TextureLibrary
{
public:
    struct Entry
    {
        std::string name;     // каноническое имя (см. выше)
        std::string folder;   // каталог относительно assets, "" для корня; для фильтра по папкам в браузере
    };

    struct Info
    {
        int width = 256;      // файла нет: считаем 256 x 256, как раньше в MapMesh
        int height = 256;
        unsigned int handle = 0;
        bool found = false;   // файл есть в индексе (иначе в handle лежит запасная текстура TextureManager)
    };

    static TextureLibrary& Get();

    // Все картинки проекта, отсортированы по имени
    const std::vector<Entry>& entries();

    // Пересканировать assets и сбросить кэш сведений (появились новые файлы или картинки подменили)
    void refresh();

    // Растёт при каждом refresh(): по нему панели понимают, что их производные списки устарели
    uint64_t revision() const { return m_revision; }

    bool contains(const std::string& name);

    // Размер и хэндл. Нет файла: found == false, размеры и хэндл запасной текстуры. Ссылка действует до refresh().
    const Info& info(const std::string& name);

    // Имя из старого формата (путь относительно <texturesRoot>, как в .map и в .cmap.json версии 1) в каноническое.
    // texturesRoot: абсолютный путь к каталогу, от которого считались старые имена (<basePath>/textures).
    // Служебные имена (IsReservedMaterial) и уровни вне assets возвращаются как есть.
    static std::string CanonicalName(const std::string& legacyName, const std::filesystem::path& texturesRoot);

    // Имена без файла: skip, clip, __TB_empty (NoDraw) и "default" (материал новых тел)
    static bool IsReservedMaterial(const std::string& name);

private:
    TextureLibrary() = default;

    void ensureIndex();

    std::vector<Entry> m_entries;
    std::unordered_set<std::string> m_names;
    bool m_indexed = false;

    std::unordered_map<std::string, Info> m_info;
    uint64_t m_revision = 1;
};
