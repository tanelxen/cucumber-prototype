//
//  AssetsPanel.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#include "AssetsPanel.h"
#include "ProjectManager.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <cstring>
#include <imgui.h>
#include <imgui_internal.h>

#include "../vendor/stb_image.h"
#include "../vendor/IconsMaterialDesignIcons.h"

namespace
{
bool IsPathWithin(const std::filesystem::path& path, const std::filesystem::path& directory)
{
    auto pathPart = path.begin();
    for (auto directoryPart = directory.begin(); directoryPart != directory.end();
         ++directoryPart, ++pathPart)
    {
        if (pathPart == path.end() || *pathPart != *directoryPart)
            return false;
    }
    return true;
}
}

AssetsPanel::AssetsPanel(const std::filesystem::path& rootPath) : EditorPanel(ICON_MDI_FOLDER_STAR " Assets###Assets"), m_rootPath(std::filesystem::absolute(rootPath))
{
    std::memset(m_searchBuffer, 0, sizeof(m_searchBuffer));
    std::memset(m_renameBuffer, 0, sizeof(m_renameBuffer));
    
    m_extensionMap[".png"]  = AssetType_Texture;
    m_extensionMap[".jpg"]  = AssetType_Texture;
    m_extensionMap[".tga"]  = AssetType_Texture;
    m_extensionMap[".glb"]  = AssetType_Mesh;
    m_extensionMap[".gltf"] = AssetType_Mesh;
    m_extensionMap[".wav"]  = AssetType_Audio;
    m_extensionMap[".mp3"]  = AssetType_Audio;
    m_extensionMap[".scene"] = AssetType_Scene;
    m_extensionMap[".json"]  = AssetType_Scene;
    m_extensionMap[".as"]   = AssetType_Script;
    m_extensionMap[".glsl"]  = AssetType_Script;
    m_extensionMap[".map"]  = AssetType_Map;
    
    m_icons[AssetType_Folder]  = loadTextureFromFile("resources/icons/folder.png");
    m_icons[AssetType_Unknown] = loadTextureFromFile("resources/icons/file_unknown.png");
    m_icons[AssetType_Texture] = loadTextureFromFile("resources/icons/file_texture.png");
    m_icons[AssetType_Mesh]    = loadTextureFromFile("resources/icons/file_mesh.png");
    m_icons[AssetType_Audio]   = loadTextureFromFile("resources/icons/file_audio.png");
    m_icons[AssetType_Scene]   = loadTextureFromFile("resources/icons/file_scene.png");
    m_icons[AssetType_Script]  = loadTextureFromFile("resources/icons/file_script.png");
    
    refresh();
}

void AssetsPanel::setRootPath(const std::filesystem::path& rootPath)
{
    m_rootPath = std::filesystem::absolute(rootPath);
    m_currentDirectory.reset();
    refresh();
}

RenderDevice::handle_t AssetsPanel::loadTextureFromFile(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path)) return 0;
    
    int width, height, channels;
    unsigned char* data = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
    
    if (!data) return 0;
    
    // Вне зависимости от каналов, запрашиваем 4 (RGBA) через stb_image, поэтому hasAlpha = true
    RenderDevice::handle_t texture = RenderDevice::makeTexture(width, height, true, true, data);
    
    stbi_image_free(data);
    return texture;
}

AssetType AssetsPanel::getAssetTypeFromExtension(const std::string& extension) const
{
    std::string extLower = extension;
    std::transform(extLower.begin(), extLower.end(), extLower.begin(), ::tolower);
    if (m_extensionMap.contains(extLower)) {
        return m_extensionMap.at(extLower);
    }
    return AssetType_Unknown;
}

void AssetsPanel::draw()
{
    if (!isVisible) return;
    
    m_isHovered = false;
    m_isFocused = false;
    
    ImGui::SetNextWindowSize(ImVec2(900, 500), ImGuiCond_FirstUseEver);
    
    if (!ImGui::Begin(m_name.c_str(), &isVisible, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar))
    {
        ImGui::End();
        return;
    }
    
    m_isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
    m_isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    
    handleShortcuts();
    
    if (ImGui::BeginTable("AssetPanelTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Outliner", ImGuiTableColumnFlags_WidthFixed, 250.0f);
        ImGui::TableSetupColumn("DirectoryContent", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        
        // 1. Левая панель: Иерархия папок
        ImGui::TableSetColumnIndex(0);
        ImGui::BeginChild("##folders_hierarchy");
        if (m_baseDirectory) {
            renderDirectoryHierarchy(m_baseDirectory);
        }
        ImGui::EndChild();
        
        // 2. Правая панель: Панель навигации и Контент
        ImGui::TableSetColumnIndex(1);
        
        float topBarHeight = 26.0f;
        float bottomBarHeight = 22.0f;
        float contentHeight = ImGui::GetWindowHeight() - topBarHeight - bottomBarHeight - ImGui::GetStyle().WindowPadding.y * 2.0f;
        
        renderTopBar(topBarHeight);
        ImGui::Separator();
        
        ImGui::BeginChild("ScrollingFilesRegion", ImVec2(0, contentHeight));
        renderGridItems();
        ImGui::EndChild();
        
        ImGui::EndTable();
    }
    
    ImGui::End();
}

void AssetsPanel::refresh()
{
    m_baseDirectory = std::make_shared<DirectoryInfo>();
    m_baseDirectory->filePath = "";
    
    processDirectory(m_rootPath, m_baseDirectory);
    
    if (m_currentDirectory)
    {
        auto found = findDirectoryByPath(m_baseDirectory, m_currentDirectory->filePath);
        m_currentDirectory = found ? found : m_baseDirectory;
    }
    else
    {
        m_currentDirectory = m_baseDirectory;
    }
    
    changeDirectory(m_currentDirectory);
}

// Если вы поддерживаете Windows и хотите фильтровать нативные скрытые файлы Windows:
#if defined(_WIN32)
#include <windows.h>
#endif

void AssetsPanel::processDirectory(const std::filesystem::path& absolutePath, std::shared_ptr<DirectoryInfo>& info)
{
    if (!std::filesystem::exists(absolutePath)) return;
    
    for (const auto& entry : std::filesystem::directory_iterator(absolutePath))
    {
        const auto& path = entry.path();
        std::string filename = path.filename().string();
        
        // 1. Универсальная проверка для macOS/Linux (и файлов вроде .git / .DS_Store на Windows)
        if (filename.empty() || filename[0] == '.') continue;
        
        // 2. Специфическая проверка для скрытых файлов Windows (атрибут hidden)
#if defined(_WIN32)
        DWORD attributes = GetFileAttributesW(path.wstring().c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_HIDDEN)) {
            continue; // Пропускаем скрытый файл Windows
        }
#endif
        
        // Если файл/папка прошли проверки, добавляем их в структуру
        if (entry.is_directory())
        {
            auto subDir = std::make_shared<DirectoryInfo>();
            subDir->filePath = std::filesystem::relative(path, m_rootPath);
            subDir->parent = info;
            
            processDirectory(path, subDir);
            info->subDirectories.push_back(subDir);
        }
        else
        {
            info->files.push_back(std::filesystem::relative(path, m_rootPath));
        }
    }
    
    // Сортировка по алфавиту
    std::sort(info->subDirectories.begin(), info->subDirectories.end(), [](const auto& a, const auto& b) {
        return a->filePath.filename().string() < b->filePath.filename().string();
    });
    std::sort(info->files.begin(), info->files.end());
}

void AssetsPanel::changeDirectory(std::shared_ptr<DirectoryInfo>& directory)
{
    if (!directory) return;
    m_currentItems.clear();
    
    if (std::strlen(m_searchBuffer) == 0)
    {
        for (auto& subdir : directory->subDirectories)
        {
            m_currentItems.push_back({
                BrowserItem::Type::Directory,
                m_rootPath / subdir->filePath,
                subdir->filePath.filename().string(),
                AssetType_Folder
            });
        }
        
        for (auto& file : directory->files)
        {
            m_currentItems.push_back({
                BrowserItem::Type::File,
                m_rootPath / file,
                file.stem().string(),
                getAssetTypeFromExtension(file.extension().string())
            });
        }
    }
    else
    {
        searchRecursive(m_searchBuffer, directory, m_currentItems);
    }
    
    std::sort(m_currentItems.begin(), m_currentItems.end(), [](const BrowserItem& a, const BrowserItem& b) {
        if (a.type != b.type) return a.type == BrowserItem::Type::Directory;
        return a.name < b.name;
    });
    
    m_previousDirectory = directory->parent;
    m_currentDirectory = directory;
    m_selectedPaths.clear();
}

void AssetsPanel::searchRecursive(const std::string& query, const std::shared_ptr<DirectoryInfo>& dir, std::vector<BrowserItem>& results) {
    std::string qLower = query;
    std::transform(qLower.begin(), qLower.end(), qLower.begin(), ::tolower);
    
    for (auto& subdir : dir->subDirectories) {
        std::string name = subdir->filePath.filename().string();
        std::string nameLower = name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        
        if (nameLower.find(qLower) != std::string::npos) {
            results.push_back({ BrowserItem::Type::Directory, m_rootPath / subdir->filePath, name });
        }
        searchRecursive(query, subdir, results);
    }
    
    for (auto& file : dir->files) {
        std::string name = file.filename().string();
        std::string nameLower = name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        
        if (nameLower.find(qLower) != std::string::npos) {
            results.push_back({ BrowserItem::Type::File, m_rootPath / file, name });
        }
    }
}

void AssetsPanel::renderDirectoryHierarchy(std::shared_ptr<DirectoryInfo>& directory)
{
    std::string name = directory->filePath == "" ? "Assets" : directory->filePath.filename().string();
    
    ImGui::PushID(name.c_str());
    
    bool isCurrent = (directory == m_currentDirectory);
    
    // Храним состояние открытости папки в локальном хранилище ImGui ID
    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImGuiID openID = ImGui::GetID("##is_open");
    bool isOpen = storage->GetBool(openID, directory->filePath == ""); // Корень открыт по умолчанию
    
    // Запоминаем начальную позицию курсора на строке
    ImVec2 startCursorPos = ImGui::GetCursorPos();
    
    // 1. Создаем невидимый Selectable на всю ширину строки.
    // Он отвечает за состояния Hovered, Selected и за кликабельность всей строки.
    ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;
    if (ImGui::Selectable("##row_selectable", isCurrent, selectableFlags, ImVec2(0, ImGui::GetFrameHeight()))) {
        m_nextDirectory = m_currentDirectory;
        changeDirectory(directory);
    }
    
    // Прием Drag-n-Drop на всю область строки папки
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_ITEM"))
        {
            AssetDragPayload* info = (AssetDragPayload*)payload->Data;
            
            std::filesystem::path src = info->path;
            std::filesystem::rename(src, m_rootPath / directory->filePath / src.filename());
            refresh();
        }
        
        ImGui::EndDragDropTarget();
    }
    
    // 2. Возвращаем курсор в начало строки, чтобы нарисовать стрелочку и иконку ПОВЕРХ Selectable
    ImGui::SetCursorPos(startCursorPos);
    
    // Включаем режим перекрытия элементов, чтобы клики проходили сквозь текст к Selectable
    ImGui::SetNextItemAllowOverlap();
    
    // 3. Отрисовка стрелочки (только если есть подпапки)
    if (!directory->subDirectories.empty()) {
        ImGuiDir arrowDir = isOpen ? ImGuiDir_Down : ImGuiDir_Right;
        
        // Убираем фоновые цвета у кнопки-стрелочки, чтобы она была прозрачной
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
        
        if (ImGui::ArrowButton("##arrow", arrowDir)) {
            isOpen = !isOpen;
            storage->SetBool(openID, isOpen);
        }
        
        ImGui::PopStyleColor(3); // Обязательно возвращаем стили назад
    }
    else {
        // Смещаем курсор вправо на ширину стрелочки для пустых папок
        ImGui::Dummy(ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
    }
    
    ImGui::SameLine();
    ImGui::SetNextItemAllowOverlap();
    
    // 4. Иконка FontAwesome (светло-голубая в стиле macOS)
    const char* iconStr = isOpen ? ICON_MDI_FOLDER_OPEN : ICON_MDI_FOLDER;
    ImVec4 macOsBlue = ImVec4(0.32f, 0.68f, 0.89f, 1.0f);
    ImGui::TextColored(macOsBlue, "%s", iconStr);
    
    ImGui::SameLine();
    ImGui::SetNextItemAllowOverlap();
    
    // 5. Просто выводим текст названия папки
    ImGui::TextUnformatted(name.c_str());
    
    // Переходим к рекурсивному рендеру подпапок, если папка раскрыта стрелочкой
    if (isOpen && !directory->subDirectories.empty()) {
        ImGui::Indent(ImGui::GetFrameHeight()); // Сдвигаем вправо дочернее дерево
        for (auto& child : directory->subDirectories) {
            renderDirectoryHierarchy(child);
        }
        ImGui::Unindent(ImGui::GetFrameHeight());
    }
    
    ImGui::PopID();
}


void AssetsPanel::renderTopBar(float height)
{
    ImGui::BeginChild("##top_bar", ImVec2(0, height));
    
    ImGui::BeginDisabled(!m_previousDirectory);
    if (ImGui::Button("<")) {
        m_nextDirectory = m_currentDirectory;
        changeDirectory(m_previousDirectory);
    }
    ImGui::EndDisabled();
    
    ImGui::SameLine();
    
    ImGui::BeginDisabled(!m_nextDirectory || m_nextDirectory == m_currentDirectory);
    if (ImGui::Button(">")) {
        changeDirectory(m_nextDirectory);
    }
    ImGui::EndDisabled();
    
    ImGui::SameLine();
    
    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::InputTextWithHint("##Search", "Search...", m_searchBuffer, sizeof(m_searchBuffer))) {
        changeDirectory(m_currentDirectory);
    }
    
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    
    std::vector<std::shared_ptr<DirectoryInfo>> crumbs;
    auto curr = m_currentDirectory;
    while (curr) {
        crumbs.push_back(curr);
        curr = curr->parent;
    }
    std::reverse(crumbs.begin(), crumbs.end());
    
    for (size_t i = 0; i < crumbs.size(); ++i) {
        std::string name = crumbs[i]->filePath == "" ? "Assets" : crumbs[i]->filePath.filename().string();
        
        if (ImGui::Selectable(name.c_str(), false, 0, ImVec2(ImGui::CalcTextSize(name.c_str()).x, 0))) {
            changeDirectory(crumbs[i]);
        }
        
        if (i < crumbs.size() - 1) {
            ImGui::SameLine();
            ImGui::TextDisabled("/");
            ImGui::SameLine();
        }
    }
    
    // Выравнивание кнопок размеров по правому краю тулбара
    // 4 кнопки по ~30px каждая + отступы между ними = ~130px свободного места справа
    ImGui::SameLine(ImGui::GetWindowWidth() - 135.0f);
    
    // Массив настроек: { Название, Размер в пикселях }
    struct SizePreset { const char* label; float pixels; ThumbnailSize sizeEnum; };
    const SizePreset presets[] = {
        { "XS", 32.0f,  ThumbnailSize::XS },
        { "S",  48.0f,  ThumbnailSize::S  },
        { "M",  64.0f,  ThumbnailSize::M  },
        { "L",  96.0f,  ThumbnailSize::L  }
    };
    
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0)); // Делаем кнопки плотнее друг к другу
    for (const auto& preset : presets) {
        bool isActive = (m_currentSizeSetting == preset.sizeEnum);
        
        // Если кнопка активна, подсвечиваем её цветом выделения (как в macOS/Unity)
        if (isActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
        } else {
            // Обычные кнопки слегка приглушаем
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_Button]);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
        }
        
        if (ImGui::Button(preset.label, ImVec2(28.0f, 0.0f))) {
            m_currentSizeSetting = preset.sizeEnum;
            m_thumbnailSize = preset.pixels;
        }
        
        ImGui::PopStyleColor(2);
        
        // Для всех кнопок кроме последней держим их на одной линии
        if (&preset != &presets[3]) {
            ImGui::SameLine();
        }
    }
    ImGui::PopStyleVar();
    
    ImGui::EndChild();
}

void AssetsPanel::renderGridItems()
{
    float padding = 12.0f;
    float cellSize = m_thumbnailSize + padding;
    float panelWidth = ImGui::GetContentRegionAvail().x;
    int columnCount = (int)(panelWidth / cellSize);
    if (columnCount < 1) columnCount = 1;
    
    ImGui::Columns(columnCount, nullptr, false);
    
    bool openDeleteModal = false;
    
    if (ImGui::BeginPopupContextWindow("GridEmptyContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
    {
        if (ImGui::BeginMenu("New"))
        {
            if (ImGui::MenuItem("Folder"))
            {
                std::filesystem::create_directory(m_rootPath / m_currentDirectory->filePath / "New Folder");
                refresh();
            }
            
            if (ImGui::MenuItem("Text File"))
            {
                std::ofstream(m_rootPath / m_currentDirectory->filePath / "NewFile.txt");
                refresh();
            }

            const auto scriptsRelativePath =
                ProjectManager::Get().scriptsPath().lexically_relative(m_rootPath).lexically_normal();
            const auto currentRelativePath = m_currentDirectory
                ? m_currentDirectory->filePath.lexically_normal()
                : std::filesystem::path{};
            const bool isInScriptsDirectory = ProjectManager::Get().hasOpenProject() &&
                !scriptsRelativePath.empty() && !scriptsRelativePath.is_absolute() &&
                IsPathWithin(currentRelativePath, scriptsRelativePath);
            if (isInScriptsDirectory && ImGui::MenuItem("Script"))
            {
                std::memset(m_scriptClassNameBuffer, 0, sizeof(m_scriptClassNameBuffer));
                m_createScriptError.clear();
                m_showCreateScriptDialog = true;
                m_openCreateScriptPopup = true;
            }
            
            ImGui::EndMenu();
        }
        
        if (ImGui::MenuItem("Refresh")) refresh();
        ImGui::EndPopup();
    }
    
    for (auto& item : m_currentItems)
    {
        ImGui::PushID(item.fullPath.string().c_str());
        
        RenderDevice::handle_t icon = m_icons.contains(item.assetType) ? m_icons[item.assetType] : m_icons[AssetType_Unknown];
        
        bool isSelected = std::find(m_selectedPaths.begin(), m_selectedPaths.end(), item.fullPath) != m_selectedPaths.end();
        
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        }
        
        // Рисуем иконку (Кнопку)
        if (icon != 0)
        {
            ImVec4 tintColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            if (item.type == BrowserItem::Type::Directory) {
                tintColor = ImVec4(0.32f, 0.68f, 0.89f, 1.0f);
            }
            
            ImGui::ImageButton("##btn", (ImTextureID)(uintptr_t)icon, ImVec2(m_thumbnailSize, m_thumbnailSize), ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0), tintColor);
        }
        else
        {
            ImGui::Button(item.type == BrowserItem::Type::Directory ? "[DIR]" : "[FILE]", ImVec2(m_thumbnailSize, m_thumbnailSize));
        }
        
        ImGui::PopStyleColor();
        
        // ЛОГИКА 1: Клик по самой ИКОНКЕ (Выделение / Вход в папку)
        if (ImGui::IsItemHovered())
        {
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                if (item.type == BrowserItem::Type::Directory) {
                    auto found = findDirectoryByPath(m_baseDirectory, std::filesystem::relative(item.fullPath, m_rootPath));
                    if (found) changeDirectory(found);
                }
            }
            else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                if (ImGui::GetIO().KeyCtrl) {
                    if (isSelected) m_selectedPaths.erase(std::remove(m_selectedPaths.begin(), m_selectedPaths.end(), item.fullPath), m_selectedPaths.end());
                    else m_selectedPaths.push_back(item.fullPath);
                } else {
                    m_selectedPaths = { item.fullPath };
                }
                m_lastClickedPath = item.fullPath;
            }
        }
        
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            AssetDragPayload info;
            info.type = item.assetType;
            strncpy(info.path, item.fullPath.string().c_str(), sizeof(info.path));
            
            ImGui::SetDragDropPayload("ASSET_ITEM", &info, sizeof(AssetDragPayload));
            
            ImGui::Text("%s", item.name.c_str());
            ImGui::EndDragDropSource();
        }
        
        // Прием Drag-n-Drop на всю область строки папки
        if (item.type == BrowserItem::Type::Directory && ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_ITEM"))
            {
                AssetDragPayload* info = (AssetDragPayload*)payload->Data;
                
                std::filesystem::path src = info->path;
                std::filesystem::rename(src, item.fullPath / src.filename());
                refresh();
            }
            
            ImGui::EndDragDropTarget();
        }
        
        if (ImGui::BeginPopupContextItem("ItemContext"))
        {
            if (!isSelected) m_selectedPaths = { item.fullPath };
            
            if (ImGui::MenuItem("Rename"))
            {
                m_pathToRename = item.fullPath;
                // При начале переименования в буфер пишем ТОЛЬКО имя, без расширения
                std::snprintf(m_renameBuffer, sizeof(m_renameBuffer), "%s", item.name.c_str());
                m_focusRenameInput = true;
            }
            
            if (ImGui::MenuItem("Открыть в Finder")) openInExplorer(item.fullPath);
            ImGui::Separator();
            if (ImGui::MenuItem("Delete")) openDeleteModal = true;
            ImGui::EndPopup();
        }
        
        // --- Переходим к отрисовке названия (уменьшаем шрифт) ---
        ImGui::SetWindowFontScale(0.85f);
        
        if (m_pathToRename == item.fullPath)
        {
            ImGui::SetNextItemWidth(m_thumbnailSize + padding);
            if (m_focusRenameInput) { ImGui::SetKeyboardFocusHere(); m_focusRenameInput = false; }
            
            bool shouldRename = false;
            if (ImGui::InputText("##rename", m_renameBuffer, sizeof(m_renameBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
                shouldRename = true;
            }
            if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                shouldRename = true;
            }
            
            if (shouldRename)
            {
                if (std::strlen(m_renameBuffer) > 0 && item.name != m_renameBuffer)
                {
                    std::filesystem::path newPath = item.fullPath.parent_path() / m_renameBuffer;
                    if (item.type == BrowserItem::Type::File) {
                        newPath.replace_extension(item.fullPath.extension());
                    }
                    std::filesystem::rename(item.fullPath, newPath);
                }
                
                m_pathToRename.clear();
                refresh();
            }
        }
        else
        {
            // Вывод отцентрированного текста без расширения
            std::string text = item.name;
            float textWidth = ImGui::CalcTextSize(text.c_str()).x;
            float columnWidth = ImGui::GetColumnWidth();
            
            if (textWidth < columnWidth) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (columnWidth - textWidth) * 0.5f - ImGui::GetStyle().ItemSpacing.x);
            }
            ImGui::TextWrapped("%s", text.c_str());
            
            // Одиночный клик по тексту для активации Finder-Style переименования
            if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                if (isSelected && m_lastClickedPath == item.fullPath)
                {
                    m_pathToRename = item.fullPath;
                    std::snprintf(m_renameBuffer, sizeof(m_renameBuffer), "%s", item.name.c_str());
                    m_focusRenameInput = true;
                }
                else
                {
                    m_selectedPaths = { item.fullPath };
                    m_lastClickedPath = item.fullPath;
                }
            }
        }
        
        ImGui::SetWindowFontScale(1.0f);
        
        ImGui::NextColumn();
        ImGui::PopID();
    }
    
    ImGui::Columns(1);
    
    renderCreateScriptDialog();
    if (openDeleteModal) ImGui::OpenPopup("Delete Items?");
    renderDeleteDialog();
}

void AssetsPanel::renderCreateScriptDialog()
{
    if (!m_showCreateScriptDialog) return;

    if (m_openCreateScriptPopup)
    {
        ImGui::OpenPopup("Create Script");
        m_openCreateScriptPopup = false;
    }

    ImGui::SetNextWindowSize(ImVec2(420.0f, 230.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Create Script", &m_showCreateScriptDialog,
                                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
        return;

    ImGui::TextWrapped("Create an EntityScript in the current scripts folder:");
    ImGui::Spacing();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Class name:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##ClassName", m_scriptClassNameBuffer,
                     sizeof(m_scriptClassNameBuffer));

    if (!m_createScriptError.empty())
    {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", m_createScriptError.c_str());
    }

    ImGui::Spacing();
    if (ImGui::Button("Create", ImVec2(120.0f, 0.0f)))
    {
        const std::string className = m_scriptClassNameBuffer;
        const bool validStart = !className.empty() &&
            (std::isalpha(static_cast<unsigned char>(className.front())) ||
             className.front() == '_');
        const bool validRemainder = std::all_of(
            className.begin(), className.end(), [](unsigned char character)
            {
                return std::isalnum(character) || character == '_';
            });

        if (!validStart || !validRemainder)
        {
            m_createScriptError =
                "Class name must start with a letter or underscore and contain only letters, digits, and underscores.";
        }
        else
        {
            const auto scriptsRelativePath =
                ProjectManager::Get().scriptsPath().lexically_relative(m_rootPath).lexically_normal();
            const auto currentRelativePath = m_currentDirectory
                ? m_currentDirectory->filePath.lexically_normal()
                : std::filesystem::path{};
            if (!ProjectManager::Get().hasOpenProject() || scriptsRelativePath.empty() ||
                scriptsRelativePath.is_absolute() ||
                !IsPathWithin(currentRelativePath, scriptsRelativePath))
            {
                m_createScriptError = "Scripts can only be created inside assets/scripts.";
                ImGui::EndPopup();
                return;
            }

            const std::filesystem::path targetDirectory = m_rootPath / currentRelativePath;
            const std::filesystem::path scriptPath = targetDirectory / (className + ".as");
            std::error_code filesystemError;
            const bool scriptExists = std::filesystem::exists(scriptPath, filesystemError);
            if (filesystemError)
            {
                m_createScriptError =
                    "Could not check the script path: " + filesystemError.message();
            }
            else if (scriptExists)
            {
                m_createScriptError = "A script with this name already exists.";
            }
            else
            {
                std::ofstream scriptFile(scriptPath, std::ios::out | std::ios::trunc);
                if (!scriptFile)
                {
                    m_createScriptError = "Could not create the script file.";
                }
                else
                {
                    scriptFile << "class " << className << " : EntityScript\n"
                               << "{\n"
                               << "    void start()\n"
                               << "    {\n"
                               << "    }\n\n"
                               << "    void update(float dt)\n"
                               << "    {\n"
                               << "    }\n"
                               << "}\n";
                    scriptFile.close();
                    if (!scriptFile)
                    {
                        m_createScriptError = "Failed while writing the script file.";
                    }
                    else
                    {
                        refresh();
                        m_showCreateScriptDialog = false;
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f)))
    {
        m_showCreateScriptDialog = false;
        m_createScriptError.clear();
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}


void AssetsPanel::renderDeleteDialog()
{
    if (ImGui::BeginPopupModal("Delete Items?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Are you sure you want to delete %d selected items?", (int)m_selectedPaths.size());
        
        if (ImGui::Button("Yes", ImVec2(120, 0)))
        {
            for (const auto& path : m_selectedPaths) {
                std::filesystem::remove_all(path);
            }
            
            m_selectedPaths.clear();
            refresh();
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("No", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void AssetsPanel::handleShortcuts()
{
    if (!m_isFocused) return;
    if (ImGui::IsKeyPressed(ImGuiKey_Delete) && !m_selectedPaths.empty() && m_pathToRename.empty()) {
        ImGui::OpenPopup("Delete Items?");
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        m_selectedPaths.clear();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F5)) {
        refresh();
    }
}

std::shared_ptr<AssetsPanel::DirectoryInfo> AssetsPanel::findDirectoryByPath(const std::shared_ptr<DirectoryInfo>& root, const std::filesystem::path& path)
{
    if (root->filePath == path) return root;
    for (auto& subdir : root->subDirectories) {
        auto found = findDirectoryByPath(subdir, path);
        if (found) return found;
    }
    return nullptr;
}

void AssetsPanel::openInExplorer(const std::filesystem::path& path)
{
#if defined(__APPLE__)
    std::string command = "open \"" + path.string() + "\"";
    std::system(command.c_str());
#elif defined(_WIN32)
    std::string command = "explorer /select,\"" + path.string() + "\"";
    std::system(command.c_str());
#endif
}
