//
//  TextureBrowserPanel.h
//  Cucumber
//
//  Браузер текстур проекта для левелдизайна: папки слева, сетка превью справа, фильтр по имени.
//  Клик по текстуре назначает её выбранным граням (через FaceTexturing::SetMaterial, то есть в историю undo).
//  Без режима правки геометрии панель работает как обычный просмотр.
//

#pragma once

#include "EditorPanel.h"

#include <cstdint>
#include <string>
#include <vector>

class TextureBrowserPanel : public EditorPanel
{
public:
    explicit TextureBrowserPanel(const std::string& name);

    void update(float dt) override { (void)dt; }
    void draw() override;

    // Показать вкладку поверх остальных в её доке (панель должна быть видимой)
    void requestFocus() { m_focusRequested = true; }

private:
    enum class Source { All, InLevel, Folder };

    struct FolderItem
    {
        std::string path;    // относительно assets, '/'
        std::string label;   // последний сегмент
        int depth = 0;
    };

    void rebuildFolders();
    bool folderContains(const std::string& folder, const std::string& entryFolder) const;
    void drawSidebar();
    // Клик кладёт имя в pending; применяется в конце кадра (SetMaterial может переаллоцировать таблицу материалов)
    void drawGrid(const std::vector<const std::string*>& names, const std::string& highlighted);

    std::vector<FolderItem> m_folders;
    uint64_t m_foldersRevision = 0;

    Source m_source = Source::All;
    std::string m_folder;                 // для Source::Folder

    char m_filter[128] = "";
    float m_thumbnailSize = 88.0f;
    float m_sidebarWidth = 180.0f;

    std::string m_pending;                // выбранная в этом кадре текстура
    std::string m_picked;                 // последняя выбранная; подсвечивается, когда нет граней для назначения

    bool m_focusRequested = false;
};
