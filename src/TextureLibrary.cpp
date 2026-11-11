//
//  TextureLibrary.cpp
//  Cucumber
//

#include "TextureLibrary.h"

#include "ProjectManager.h"
#include "TextureManager.h"

#include <algorithm>
#include <cctype>

namespace
{
    // Должен совпадать со списком в TextureManager::loadTexture (там расширения перебираются для имён без расширения).
    // Если добавляете формат, например .bmp, добавьте его в обоих местах.
    bool HasImageExtension(const std::filesystem::path& path)
    {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga";
    }

    bool IsHidden(const std::filesystem::path& path)
    {
        const std::string name = path.filename().string();
        return !name.empty() && name[0] == '.';
    }
}

TextureLibrary& TextureLibrary::Get()
{
    static TextureLibrary instance;
    return instance;
}

const std::vector<TextureLibrary::Entry>& TextureLibrary::entries()
{
    ensureIndex();
    return m_entries;
}

void TextureLibrary::refresh()
{
    m_indexed = false;
    m_info.clear();
    ++m_revision;
    ensureIndex();
}

bool TextureLibrary::contains(const std::string& name)
{
    ensureIndex();
    return m_names.count(name) != 0;
}

const TextureLibrary::Info& TextureLibrary::info(const std::string& name)
{
    const auto it = m_info.find(name);
    if (it != m_info.end()) return it->second;

    Info info;
    info.found = contains(name);

    // Нет файла: TextureManager вернёт запасную текстуру, берём её размеры и хэндл (так материал не остаётся пустым)
    if (auto asset = TextureManager::Get().loadTexture(name))
    {
        info.width = asset->getWidth();
        info.height = asset->getHeight();
        info.handle = asset->getHandle();
    }
    return m_info.emplace(name, info).first->second;
}

void TextureLibrary::ensureIndex()
{
    if (m_indexed) return;
    m_indexed = true;

    m_entries.clear();
    m_names.clear();

    const std::filesystem::path root = ProjectManager::Get().assetsPath();
    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) return;

    for (std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec))
    {
        // Скрытые каталоги (.git, .DS_Store и подобные) не заходим
        if (IsHidden(it->path()))
        {
            if (it->is_directory(ec)) it.disable_recursion_pending();
            ec.clear();
            continue;
        }
        if (!it->is_regular_file(ec) || !HasImageExtension(it->path())) { ec.clear(); continue; }

        std::filesystem::path relative = std::filesystem::relative(it->path(), root, ec);
        if (ec) { ec.clear(); continue; }

        std::filesystem::path folder = relative.parent_path();
        relative.replace_extension();

        Entry entry;
        entry.name = relative.generic_string();
        entry.folder = folder.generic_string();
        if (m_names.insert(entry.name).second) m_entries.push_back(std::move(entry));
    }

    std::sort(m_entries.begin(), m_entries.end(), [](const Entry& a, const Entry& b) { return a.name < b.name; });
}

bool TextureLibrary::IsReservedMaterial(const std::string& name)
{
    return name == "__TB_empty" || name == "skip" || name == "clip" || name == "default";
}

std::string TextureLibrary::CanonicalName(const std::string& legacyName, const std::filesystem::path& texturesRoot)
{
    if (IsReservedMaterial(legacyName)) return legacyName;

    std::error_code ec;
    const std::filesystem::path prefix = std::filesystem::relative(texturesRoot, ProjectManager::Get().assetsPath(), ec);

    // Каталог текстур вне assets (или путь не удалось вычислить): выразить имя нельзя, оставляем прежнее
    if (ec || prefix.empty() || *prefix.begin() == "..") return legacyName;

    return (prefix / legacyName).lexically_normal().generic_string();
}
