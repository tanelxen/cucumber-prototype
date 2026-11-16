//
//  LevelFiles.cpp
//  Cucumber
//

#include "LevelFiles.h"

#include "core/ProjectManager.h"
#include "TextureLibrary.h"

#include "assets/MapAsset.h"
#include "assets/MapImporter.h"
#include "level/LevelDocument.h"
#include "level/LevelDocumentIO.h"

#include <iostream>
#include <string_view>

namespace LevelFiles
{
    bool IsLevelFile(const std::string& path)
    {
        constexpr std::string_view ext = ".cmap.json";
        return path.size() >= ext.size() && path.compare(path.size() - ext.size(), ext.size(), ext) == 0;
    }

    std::filesystem::path LevelPathFor(std::filesystem::path mapFile)
    {
        return mapFile.replace_extension(".cmap.json");
    }

    std::filesystem::path NextUntitledPath()
    {
        const auto assets = ProjectManager::Get().assetsPath();

        std::filesystem::path candidate;
        for (int i = 0;; ++i)
        {
            candidate = std::string("maps/untitled") + (i ? "_" + std::to_string(i) : "") + ".cmap.json";
            if (!std::filesystem::exists(assets / candidate)) break;
        }
        return candidate;
    }

    bool Load(const std::filesystem::path& relativePath, LevelDocument& out, std::string& error)
    {
        const auto absolute = ProjectManager::Get().assetsPath() / relativePath;

        LevelDocument loaded;
        std::string ioError;
        if (!LevelDocumentIO::Load(absolute, loaded, ioError))
        {
            error = "Level load failed: " + ioError;
            return false;
        }

        out = std::move(loaded);
        return true;
    }

    bool Save(const std::filesystem::path& relativePath, const LevelDocument& level, std::string& error)
    {
        std::string ioError;
        if (!LevelDocumentIO::Save(ProjectManager::Get().assetsPath() / relativePath, level, ioError))
        {
            error = "Save failed: " + ioError;
            return false;
        }
        return true;
    }

    bool CreateEmpty(const std::filesystem::path& relativePath, std::string& error)
    {
        const auto absolute = ProjectManager::Get().assetsPath() / relativePath;
        if (std::filesystem::exists(absolute))
        {
            error = "File already exists: " + relativePath.string();
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(absolute.parent_path(), ec);

        std::string ioError;
        if (!LevelDocumentIO::Save(absolute, LevelDocument{}, ioError))
        {
            error = "Cannot create level: " + ioError;
            return false;
        }
        return true;
    }

    bool Import(const std::filesystem::path& mapFile, bool overwrite, std::filesystem::path& levelRelativePath, std::string& error)
    {
        const auto assets = ProjectManager::Get().assetsPath();
        levelRelativePath = LevelPathFor(mapFile);

        // Уже есть документ: не затираем правки редактора
        if (!overwrite && std::filesystem::exists(assets / levelRelativePath)) return true;

        MapAsset mapData;
        if (!mapData.loadFromFile((assets / mapFile).string()))
        {
            error = "Cannot open or parse .map file";
            return false;
        }

        LevelDocument imported;
        MapImportReport report;
        std::string importError;
        if (!ImportMapAsset(mapData, imported, report, importError))
        {
            error = "Map import failed: " + importError;
            return false;
        }

        std::cout << "[Import] brushes " << report.brushesImported << "/" << report.brushesRead
                  << ", skipped " << report.brushesSkipped << ", faces dropped " << report.facesDropped
                  << ", entities ignored " << report.entitiesIgnored << std::endl;
        for (const auto& warning : report.warnings) std::cout << "[Import] " << warning << std::endl;

        // generic_string: слеши одинаковые на всех платформах
        imported.sourceMap = mapFile.generic_string();

        std::string ioError;
        if (!LevelDocumentIO::Save(assets / levelRelativePath, imported, ioError))
        {
            error = "Cannot save level: " + ioError;
            return false;
        }
        return true;
    }
}
