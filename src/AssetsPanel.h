//
//  AssetsPanel.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#pragma once

#include "EditorPanel.h"
#include "RenderDevice.h"

#include <string>
#include <vector>
#include <filesystem>
#include <map>
#include <memory>

class AssetsPanel : public EditorPanel
{
public:
    struct DirectoryInfo {
        std::filesystem::path filePath; // Относительный путь от корня проекта
        std::shared_ptr<DirectoryInfo> parent = nullptr;
        std::vector<std::shared_ptr<DirectoryInfo>> subDirectories;
        std::vector<std::filesystem::path> files;
    };
    
    enum class AssetType {
        Folder,
        Unknown,
        Texture,
        Mesh,
        Audio,
        Scene,
        Script
    };
    
    struct BrowserItem {
        enum class Type { Directory, File } type;
        std::filesystem::path fullPath;
        std::string name;
        AssetType assetType;
    };
    
    struct DragDropAssetInfo {
        AssetType type;
        char path[256];
    };
    
    AssetsPanel(const std::filesystem::path& rootPath = "assets");
    virtual ~AssetsPanel() = default;
    
    virtual void draw() override;
    void refresh();
    
private:
    void processDirectory(const std::filesystem::path& absolutePath, std::shared_ptr<DirectoryInfo>& info);
    void changeDirectory(std::shared_ptr<DirectoryInfo>& directory);
    void searchRecursive(const std::string& query, const std::shared_ptr<DirectoryInfo>& dir, std::vector<BrowserItem>& results);
    
    void renderDirectoryHierarchy(std::shared_ptr<DirectoryInfo>& directory);
    void renderTopBar(float height);
    void renderGridItems();
    void renderDeleteDialog();
    void renderBottomBar(float height);
    void handleShortcuts();
    
    RenderDevice::handle_t loadTextureFromFile(const std::filesystem::path& path);
    std::shared_ptr<DirectoryInfo> findDirectoryByPath(const std::shared_ptr<DirectoryInfo>& root, const std::filesystem::path& path);
    AssetType getAssetTypeFromExtension(const std::string& extension) const;
    
private:
    std::filesystem::path m_rootPath;
    std::shared_ptr<DirectoryInfo> m_baseDirectory = nullptr;
    std::shared_ptr<DirectoryInfo> m_currentDirectory = nullptr;
    
    std::shared_ptr<DirectoryInfo> m_nextDirectory = nullptr;
    std::shared_ptr<DirectoryInfo> m_previousDirectory = nullptr;
    
    std::vector<BrowserItem> m_currentItems;
    std::vector<std::filesystem::path> m_selectedPaths;
    
    std::filesystem::path m_pathToRename;
    char m_renameBuffer[256];
    char m_searchBuffer[128];
    
    // Хендлы текстур-иконок
    std::map<AssetType, RenderDevice::handle_t> m_icons;
    std::map<std::string, AssetType> m_extensionMap;
    
    bool m_isHovered = false;
    bool m_isFocused = false;
    
    enum class ThumbnailSize {
        XS = 0,
        S,
        M,
        L
    };
    
    ThumbnailSize m_currentSizeSetting = ThumbnailSize::M; // Размер по умолчанию
    float m_thumbnailSize = 64.0f; // Оставляем для внутренних расчетов сетки
    
private:
    // Переменные для Finder-style переименования
    std::filesystem::path m_lastClickedPath;
    float m_lastClickTime = 0.0f;
    bool m_focusRenameInput = false;
    
    // Метод для нативного вызова Finder / Проводника
    void openInExplorer(const std::filesystem::path& path);
};
