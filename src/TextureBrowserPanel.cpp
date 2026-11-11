//
//  TextureBrowserPanel.cpp
//  Cucumber
//

#include "TextureBrowserPanel.h"

#include "MapComponent.h"
#include "SceneManager.h"
#include "TextureLibrary.h"
#include "World.h"
#include "level/FaceTexturing.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <utility>

namespace
{
    ImTextureID ToImGuiTexture(unsigned int handle)
    {
        return (ImTextureID)(intptr_t)handle;   // handle_t это имя GL-текстуры (так же во ViewportPanel)
    }

    std::string ToLower(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }

    // needle уже в нижнем регистре; haystack не копируется (в каталоге могут быть тысячи имён)
    bool ContainsNoCase(const std::string& haystack, const std::string& needle)
    {
        if (needle.empty()) return true;
        return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
                           [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == b; }) != haystack.end();
    }

    // Ключ сортировки папок: '/' меньше любого символа, иначе "a-b" встанет между "a" и "a/c"
    std::string FolderSortKey(std::string path)
    {
        std::replace(path.begin(), path.end(), '/', '\x01');
        return path;
    }
}

TextureBrowserPanel::TextureBrowserPanel(const std::string& name)
    : EditorPanel(name)
{
}

void TextureBrowserPanel::rebuildFolders()
{
    TextureLibrary& library = TextureLibrary::Get();
    m_foldersRevision = library.revision();

    // Каждая папка с текстурами тянет за собой всех предков, чтобы дерево было связным
    std::vector<std::pair<std::string, std::string>> keyed;   // (ключ сортировки, путь)
    std::vector<std::string> seen;
    for (const TextureLibrary::Entry& entry : library.entries())
    {
        std::string folder = entry.folder;
        while (!folder.empty())
        {
            if (std::find(seen.begin(), seen.end(), folder) != seen.end()) break;
            seen.push_back(folder);
            keyed.emplace_back(FolderSortKey(folder), folder);

            const size_t slash = folder.find_last_of('/');
            folder = slash == std::string::npos ? std::string() : folder.substr(0, slash);
        }
    }
    std::sort(keyed.begin(), keyed.end());

    m_folders.clear();
    for (const auto& item : keyed)
    {
        FolderItem folder;
        folder.path = item.second;
        const size_t slash = folder.path.find_last_of('/');
        folder.label = slash == std::string::npos ? folder.path : folder.path.substr(slash + 1);
        folder.depth = static_cast<int>(std::count(folder.path.begin(), folder.path.end(), '/'));
        m_folders.push_back(std::move(folder));
    }

    // Выбранной папки больше нет (удалили или переименовали)
    if (m_source == Source::Folder &&
        std::none_of(m_folders.begin(), m_folders.end(), [&](const FolderItem& f) { return f.path == m_folder; }))
    {
        m_source = Source::All;
        m_folder.clear();
    }
}

bool TextureBrowserPanel::folderContains(const std::string& folder, const std::string& entryFolder) const
{
    if (entryFolder == folder) return true;
    return entryFolder.size() > folder.size()
        && entryFolder.compare(0, folder.size(), folder) == 0
        && entryFolder[folder.size()] == '/';
}

void TextureBrowserPanel::drawSidebar()
{
    TextureLibrary& library = TextureLibrary::Get();

    if (ImGui::BeginChild("##texture_folders", ImVec2(m_sidebarWidth, 0), true))
    {
        const std::string all = "All textures (" + std::to_string(library.entries().size()) + ")";
        if (ImGui::Selectable(all.c_str(), m_source == Source::All)) m_source = Source::All;
        if (ImGui::Selectable("In this level", m_source == Source::InLevel)) m_source = Source::InLevel;
        ImGui::Separator();

        for (size_t i = 0; i < m_folders.size(); ++i)
        {
            const FolderItem& folder = m_folders[i];
            const bool selected = m_source == Source::Folder && m_folder == folder.path;

            ImGui::PushID(static_cast<int>(i));
            if (folder.depth > 0) ImGui::Indent(10.0f * folder.depth);
            if (ImGui::Selectable(folder.label.c_str(), selected))
            {
                m_source = Source::Folder;
                m_folder = folder.path;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", folder.path.c_str());
            if (folder.depth > 0) ImGui::Unindent(10.0f * folder.depth);
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

void TextureBrowserPanel::drawGrid(const std::vector<const std::string*>& names, const std::string& highlighted)
{
    TextureLibrary& library = TextureLibrary::Get();

    if (!ImGui::BeginChild("##texture_grid", ImVec2(0, 0), true)) { ImGui::EndChild(); return; }

    const float cell = m_thumbnailSize;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float labelHeight = ImGui::GetTextLineHeight();
    const int columns = std::max(1, static_cast<int>((ImGui::GetContentRegionAvail().x + spacing) / (cell + spacing)));
    const int rows = (static_cast<int>(names.size()) + columns - 1) / columns;

    // Текстуры грузятся только для видимых строк: в проекте их могут быть тысячи
    ImGuiListClipper clipper;
    clipper.Begin(rows, cell + labelHeight + ImGui::GetStyle().ItemSpacing.y);
    while (clipper.Step())
    {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
        {
            for (int column = 0; column < columns; ++column)
            {
                const int index = row * columns + column;
                if (index >= static_cast<int>(names.size())) break;
                if (column > 0) ImGui::SameLine();

                const std::string& name = *names[index];
                // Только превью: полноразмерная текстура через info() осталась бы в GPU навсегда
                const TextureLibrary::Preview& preview = library.preview(name);
                using State = TextureLibrary::Preview::State;

                ImGui::PushID(index);
                ImGui::BeginGroup();

                const ImVec2 position = ImGui::GetCursorScreenPos();
                const ImVec2 corner(position.x + cell, position.y + cell);
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                if (preview.state == State::Ready)
                {
                    // Вписываем с сохранением пропорций оригинала
                    const float aspect = preview.height > 0 ? static_cast<float>(preview.width) / preview.height : 1.0f;
                    const ImVec2 size = aspect >= 1.0f ? ImVec2(cell, cell / aspect) : ImVec2(cell * aspect, cell);
                    ImGui::SetCursorScreenPos(ImVec2(position.x + (cell - size.x) * 0.5f, position.y + (cell - size.y) * 0.5f));
                    ImGui::Image(ToImGuiTexture(preview.handle), size);
                }
                else if (preview.state == State::Loading)
                {
                    drawList->AddRectFilled(position, corner, IM_COL32(45, 45, 50, 255));   // заглушка, пока декодируется
                }
                else
                {
                    drawList->AddRectFilled(position, corner, IM_COL32(110, 40, 40, 255));  // нет файла или не читается
                }

                ImGui::SetCursorScreenPos(position);
                if (ImGui::InvisibleButton("##texture", ImVec2(cell, cell))) m_pending = name;

                if (ImGui::IsItemHovered())
                {
                    switch (preview.state)
                    {
                        case State::Ready:    ImGui::SetTooltip("%s\n%d x %d", name.c_str(), preview.width, preview.height); break;
                        case State::Loading:  ImGui::SetTooltip("%s", name.c_str()); break;
                        case State::Failed:   ImGui::SetTooltip("%s\nfailed to load", name.c_str()); break;
                        case State::NotFound: ImGui::SetTooltip("%s\nfile not found", name.c_str()); break;
                    }
                }
                if (!highlighted.empty() && name == highlighted)
                    drawList->AddRect(position, ImVec2(position.x + cell, position.y + cell), IM_COL32(255, 160, 30, 255), 0.0f, 0, 2.0f);

                // Подпись: имя файла без каталога, обрезанное по ширине ячейки
                const size_t slash = name.find_last_of('/');
                const char* shortName = name.c_str() + (slash == std::string::npos ? 0 : slash + 1);
                ImGui::PushClipRect(position, ImVec2(position.x + cell, position.y + cell + labelHeight + 2), true);
                ImGui::SetCursorScreenPos(ImVec2(position.x, position.y + cell));
                ImGui::TextUnformatted(shortName);
                ImGui::PopClipRect();

                ImGui::EndGroup();
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();
}

void TextureBrowserPanel::draw()
{
    if (m_focusRequested)
    {
        ImGui::SetNextWindowFocus();
        m_focusRequested = false;
    }

    if (!ImGui::Begin(m_name.c_str()))
    {
        ImGui::End();
        return;
    }

    TextureLibrary& library = TextureLibrary::Get();
    // Раз в кадр до первого preview(): заливка готовых превью в GPU, снятие устаревших заданий, выгрузка по бюджету
    library.pumpPreviews(static_cast<uint64_t>(ImGui::GetFrameCount()));

    auto* scene = SceneManager::Get().getActiveScene();
    MapComponent* map = scene ? scene->getMap() : nullptr;
    LevelEditSession* session = map ? map->editSession() : nullptr;

    // ---- Что сейчас можно текстурировать ----
    std::vector<FaceRef> targets;
    FaceTexturing::Summary summary;
    bool hasTargets = false;
    if (session)
    {
        targets = session->textureTargets();
        hasTargets = !targets.empty() && FaceTexturing::Summarize(*session, targets, summary);
    }

    std::string highlighted;
    if (hasTargets)
    {
        if (!summary.mixedMaterial) highlighted = session->document().materials[summary.material];
    }
    else
    {
        highlighted = m_picked;
    }

    // ---- Верхняя строка ----
    ImGui::SetNextItemWidth(-190.0f);
    ImGui::InputTextWithHint("##filter", "Filter textures", m_filter, sizeof(m_filter));
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) library.refresh();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70.0f);
    ImGui::DragFloat("##thumb", &m_thumbnailSize, 1.0f, 32.0f, static_cast<float>(TextureLibrary::kPreviewEdge), "%.0f px");

    if (hasTargets)    ImGui::TextDisabled("Click a texture to apply it to %d face(s)", summary.total);
    else if (session)  ImGui::TextDisabled("Select a solid (click) or faces (Alt+click) to apply textures");
    else               ImGui::TextDisabled("Geometry edit mode is off: browsing only");

    if (m_foldersRevision != library.revision()) rebuildFolders();

    // ---- Список для сетки ----
    // Указатели на имена живут до конца кадра: каталог меняет только refresh() (он выше), таблицу материалов документа
    // только применение выбора (оно ниже, после отрисовки).
    std::vector<const std::string*> names;
    const std::string needle = ToLower(m_filter);
    if (m_source == Source::InLevel)
    {
        if (session)
        {
            for (const std::string& name : session->document().materials)
            {
                if (!TextureLibrary::IsReservedMaterial(name) && ContainsNoCase(name, needle)) names.push_back(&name);
            }
        }
    }
    else
    {
        for (const TextureLibrary::Entry& entry : library.entries())
        {
            if (m_source == Source::Folder && !folderContains(m_folder, entry.folder)) continue;
            if (ContainsNoCase(entry.name, needle)) names.push_back(&entry.name);
        }
    }

    // ---- Тело ----
    drawSidebar();
    ImGui::SameLine();
    if (m_source == Source::InLevel && !session)
    {
        ImGui::BeginGroup();
        ImGui::TextWrapped("Level materials are available in geometry edit mode.");
        ImGui::EndGroup();
    }
    else
    {
        drawGrid(names, highlighted);
    }

    // Выбор применяем в конце кадра: SetMaterial может добавить материал в таблицу документа (реаллокация)
    if (!m_pending.empty())
    {
        m_picked = m_pending;
        if (hasTargets) FaceTexturing::SetMaterial(*session, targets, m_pending);
        m_pending.clear();
    }

    ImGui::End();
}
