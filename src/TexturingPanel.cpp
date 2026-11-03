//
//  TexturingPanel.cpp
//  Cucumber
//

#include "TexturingPanel.h"

#include "MapComponent.h"
#include "ProjectManager.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "World.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>

namespace
{
    // Тот же корень, что жёстко зашит в MapMesh ("wolf/textures/" + имя материала)
    constexpr const char* kTextureRoot = "wolf/textures";

    bool HasImageExtension(const std::filesystem::path& path)
    {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp";
    }

    ImTextureID ToImGuiTexture(unsigned int handle)
    {
        return (ImTextureID)(intptr_t)handle;   // handle_t это имя GL-текстуры (так же во ViewportPanel)
    }

    // Сводка по целям: значения первой грани и признак, что у остальных граней они отличаются.
    // Хранит копии, а не указатели: любая правка внутри кадра переприсваивает грани документа, и указатели повисли бы
    // к следующему виджету.
    struct Summary
    {
        uint32_t material = 0;
        FaceUV uv;
        float rotation = 0.0f;

        bool mixedMaterial = false;
        bool mixedShift = false;
        bool mixedScale = false;
        bool mixedRotation = false;
        int noDrawCount = 0;
        int total = 0;
    };

    bool Summarize(const LevelEditSession& session, const std::vector<FaceRef>& targets, Summary& out)
    {
        const Solid* firstSolid = session.findSolid(targets.front().solidUid);
        const Face* firstFace = session.findFace(targets.front());
        if (!firstSolid || !firstFace) return false;

        out.material = firstFace->material;
        out.uv = firstFace->uv;
        out.rotation = FaceTexturing::Rotation(FaceNormal(*firstSolid, *firstFace), firstFace->uv);

        for (const FaceRef& ref : targets)
        {
            const Solid* solid = session.findSolid(ref.solidUid);
            const Face* face = session.findFace(ref);
            if (!solid || !face) continue;

            ++out.total;
            if (face->flags & FaceFlag::NoDraw) ++out.noDrawCount;

            out.mixedMaterial = out.mixedMaterial || face->material != out.material;
            out.mixedShift = out.mixedShift || face->uv.shift != out.uv.shift;
            out.mixedScale = out.mixedScale || face->uv.scale != out.uv.scale;

            const float rotation = FaceTexturing::Rotation(FaceNormal(*solid, *face), face->uv);
            out.mixedRotation = out.mixedRotation || std::abs(FaceTexturing::NormalizeDegrees(rotation - out.rotation)) > 1e-3f;
        }
        return true;
    }

    // Подпись поля: звёздочка, если у выбранных граней значения различаются
    std::string Label(const char* text, bool mixed)
    {
        return mixed ? std::string(text) + " *" : std::string(text);
    }
}

TexturingPanel::TexturingPanel(const std::string& name)
    : EditorPanel(name)
{
}

const TexturingPanel::TextureInfo& TexturingPanel::texture(const std::string& name)
{
    const auto it = m_textures.find(name);
    if (it != m_textures.end()) return it->second;

    TextureInfo info;
    if (auto asset = TextureManager::Get().loadTexture(std::string(kTextureRoot) + "/" + name))
    {
        info.width = asset->getWidth();
        info.height = asset->getHeight();
        info.handle = asset->getHandle();
    }
    return m_textures.emplace(name, info).first->second;
}

void TexturingPanel::refreshCatalog()
{
    m_catalog.clear();
    m_catalogLoaded = true;

    const std::filesystem::path root = ProjectManager::Get().assetsPath() / kTextureRoot;
    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) return;

    for (std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec))
    {
        if (!it->is_regular_file(ec) || !HasImageExtension(it->path())) continue;

        std::filesystem::path relative = std::filesystem::relative(it->path(), root, ec);
        if (ec) { ec.clear(); continue; }

        relative.replace_extension();
        m_catalog.push_back(relative.generic_string());
    }

    std::sort(m_catalog.begin(), m_catalog.end());
    m_catalog.erase(std::unique(m_catalog.begin(), m_catalog.end()), m_catalog.end());
}

bool TexturingPanel::materialMatchesFilter(const std::string& name) const
{
    if (m_filter[0] == '\0') return true;

    std::string haystack = name;
    std::string needle = m_filter;
    std::transform(haystack.begin(), haystack.end(), haystack.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return haystack.find(needle) != std::string::npos;
}

void TexturingPanel::drawMaterialGrid(const char* id, const std::vector<std::string>& names, const std::string& current, float height)
{
    if (!ImGui::BeginChild(id, ImVec2(0, height), true)) { ImGui::EndChild(); return; }

    const float cell = m_thumbnailSize;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float labelHeight = ImGui::GetTextLineHeight();
    const int columns = std::max(1, static_cast<int>((ImGui::GetContentRegionAvail().x + spacing) / (cell + spacing)));
    const int rows = (static_cast<int>(names.size()) + columns - 1) / columns;

    // Текстуры грузятся только для видимых строк: каталог может содержать тысячи файлов
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

                const std::string& name = names[index];
                const TextureInfo& info = texture(name);

                ImGui::PushID(index);
                ImGui::BeginGroup();

                const ImVec2 position = ImGui::GetCursorScreenPos();
                if (info.handle != 0)
                {
                    // Вписываем с сохранением пропорций
                    const float aspect = info.height > 0 ? static_cast<float>(info.width) / info.height : 1.0f;
                    const ImVec2 size = aspect >= 1.0f ? ImVec2(cell, cell / aspect) : ImVec2(cell * aspect, cell);
                    ImGui::SetCursorScreenPos(ImVec2(position.x + (cell - size.x) * 0.5f, position.y + (cell - size.y) * 0.5f));
                    ImGui::Image(ToImGuiTexture(info.handle), size);
                }
                else
                {
                    ImGui::GetWindowDrawList()->AddRectFilled(position, ImVec2(position.x + cell, position.y + cell), IM_COL32(60, 40, 40, 255));
                }

                ImGui::SetCursorScreenPos(position);
                if (ImGui::InvisibleButton("##material", ImVec2(cell, cell))) m_pendingMaterial = name;   // применяем в конце кадра

                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\n%d x %d", name.c_str(), info.width, info.height);
                if (name == current)
                {
                    ImGui::GetWindowDrawList()->AddRect(position, ImVec2(position.x + cell, position.y + cell), IM_COL32(255, 160, 30, 255), 0.0f, 0, 2.0f);
                }

                // Подпись: хвост имени без каталога, обрезанный по ширине ячейки
                const std::string shortName = name.substr(name.find_last_of('/') == std::string::npos ? 0 : name.find_last_of('/') + 1);
                ImGui::PushClipRect(position, ImVec2(position.x + cell, position.y + cell + labelHeight + 2), true);
                ImGui::SetCursorScreenPos(ImVec2(position.x, position.y + cell));
                ImGui::TextUnformatted(shortName.c_str());
                ImGui::PopClipRect();

                ImGui::EndGroup();
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();
}

void TexturingPanel::draw()
{
    if (!ImGui::Begin(m_name.c_str()))
    {
        ImGui::End();
        return;
    }

    auto* scene = SceneManager::Get().getActiveScene();
    MapComponent* map = scene ? scene->getMap() : nullptr;
    LevelEditSession* session = map ? map->editSession() : nullptr;

    if (!session)
    {
        ImGui::TextWrapped("Texturing is available in geometry edit mode: select the MapComponent and press Edit.");
        ImGui::End();
        return;
    }

    const std::vector<FaceRef> targets = session->textureTargets();
    if (targets.empty())
    {
        ImGui::TextWrapped("Select a solid (click) or faces (Alt+click, Ctrl+Alt+click to add).");
        ImGui::End();
        return;
    }

    Summary summary;
    if (!Summarize(*session, targets, summary))
    {
        ImGui::End();
        return;
    }

    if (!session->selectedFaces().empty()) ImGui::Text("%d selected face(s)", summary.total);
    else                                   ImGui::Text("All faces of %d solid(s) (%d)", static_cast<int>(session->selectedSolids().size()), summary.total);
    ImGui::Separator();

    // ---- Материал ----
    LevelDocument& document = session->document();
    const std::string currentName = document.materials[summary.material];
    {
        const TextureInfo& info = texture(currentName);
        if (info.handle != 0) ImGui::Image(ToImGuiTexture(info.handle), ImVec2(64, 64));
        else                  ImGui::Dummy(ImVec2(64, 64));

        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted(summary.mixedMaterial ? "(several materials)" : currentName.c_str());
        if (!summary.mixedMaterial) ImGui::TextDisabled("%d x %d", info.width, info.height);
        ImGui::EndGroup();
    }

    if (!m_catalogLoaded) refreshCatalog();

    ImGui::SetNextItemWidth(-130.0f);
    ImGui::InputTextWithHint("##filter", "Filter materials", m_filter, sizeof(m_filter));
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) refreshCatalog();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.0f);
    ImGui::DragFloat("##thumb", &m_thumbnailSize, 1.0f, 32.0f, 160.0f, "%.0f px");

    {
        // Материалы уровня первыми: в них могут быть и шейдерные имена, которых нет на диске
        std::vector<std::string> inLevel, onDisk;
        for (const std::string& name : document.materials)
        {
            if (materialMatchesFilter(name)) inLevel.push_back(name);
        }
        for (const std::string& name : m_catalog)
        {
            if (materialMatchesFilter(name) && std::find(document.materials.begin(), document.materials.end(), name) == document.materials.end())
                onDisk.push_back(name);
        }

        const float available = std::max(160.0f, ImGui::GetContentRegionAvail().y * 0.45f);

        ImGui::TextDisabled("In this level (%d)", static_cast<int>(inLevel.size()));
        drawMaterialGrid("##level_materials", inLevel, summary.mixedMaterial ? std::string() : currentName,
                         std::min(available * 0.5f, m_thumbnailSize * 2.0f + 40.0f));

        ImGui::TextDisabled("Textures (%d)", static_cast<int>(onDisk.size()));
        drawMaterialGrid("##disk_materials", onDisk, std::string(), available);
    }

    ImGui::Separator();

    // ---- Раскладка ----
    {
        glm::vec2 shift = summary.uv.shift;
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::DragFloat2(Label("Offset", summary.mixedShift).c_str(), &shift.x, 0.5f, 0.0f, 0.0f, "%.2f"))
            FaceTexturing::SetShift(*session, targets, shift, "tex.shift");
        if (ImGui::IsItemDeactivatedAfterEdit()) session->sealEdit();

        glm::vec2 scale = summary.uv.scale;
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::DragFloat2(Label("Scale", summary.mixedScale).c_str(), &scale.x, 0.01f, 0.0f, 0.0f, "%.3f"))
            FaceTexturing::SetScale(*session, targets, scale, "tex.scale");
        if (ImGui::IsItemDeactivatedAfterEdit()) session->sealEdit();

        float rotation = summary.rotation;
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::DragFloat(Label("Rotation", summary.mixedRotation).c_str(), &rotation, 0.5f, -180.0f, 180.0f, "%.1f deg"))
            FaceTexturing::SetRotation(*session, targets, rotation, "tex.rotation");
        if (ImGui::IsItemDeactivatedAfterEdit()) session->sealEdit();
    }

    // ---- Быстрые операции ----
    if (ImGui::Button("Rot -90")) FaceTexturing::Rotate(*session, targets, -90.0f);
    ImGui::SameLine();
    if (ImGui::Button("Rot +90")) FaceTexturing::Rotate(*session, targets, 90.0f);
    ImGui::SameLine();
    if (ImGui::Button("Flip U")) FaceTexturing::Flip(*session, targets, true);
    ImGui::SameLine();
    if (ImGui::Button("Flip V")) FaceTexturing::Flip(*session, targets, false);
    ImGui::SameLine();
    if (ImGui::Button("Reset")) FaceTexturing::ResetUV(*session, targets);

    // Сдвиг текстуры стрелками: направления считаются от самой текстуры (вправо = вдоль её оси u)
    const float step = m_nudgeStep > 0.0f ? m_nudgeStep : static_cast<float>(session->gridSize());
    ImGui::SetNextItemWidth(70.0f);
    ImGui::DragFloat("##step", &m_nudgeStep, 0.1f, 0.0f, 1024.0f, m_nudgeStep > 0.0f ? "%.1f" : "grid");
    ImGui::SameLine();
    if (ImGui::ArrowButton("##left", ImGuiDir_Left))   FaceTexturing::Nudge(*session, targets, { step, 0.0f });
    ImGui::SameLine();
    if (ImGui::ArrowButton("##right", ImGuiDir_Right)) FaceTexturing::Nudge(*session, targets, { -step, 0.0f });
    ImGui::SameLine();
    if (ImGui::ArrowButton("##up", ImGuiDir_Up))       FaceTexturing::Nudge(*session, targets, { 0.0f, step });
    ImGui::SameLine();
    if (ImGui::ArrowButton("##down", ImGuiDir_Down))   FaceTexturing::Nudge(*session, targets, { 0.0f, -step });
    ImGui::SameLine();
    ImGui::TextDisabled("Step");

    // Подгонка под грань: текстура ложится ровно repeat раз
    ImGui::SetNextItemWidth(110.0f);
    ImGui::DragFloat2("##repeat", &m_repeat.x, 0.1f, 0.1f, 64.0f, "%.1f");
    ImGui::SameLine();
    if (ImGui::Button("Fit to face"))
    {
        FaceTexturing::FitToFace(*session, targets, [this](const std::string& name)
        {
            const TextureInfo& info = texture(name);
            return glm::ivec2(info.width, info.height);
        }, m_repeat);
    }

    ImGui::Separator();

    // ---- Флаги ----
    {
        const bool allNoDraw = summary.noDrawCount == summary.total;
        bool noDraw = allNoDraw;
        const char* label = (summary.noDrawCount > 0 && !allNoDraw) ? "No draw (some)" : "No draw";
        if (ImGui::Checkbox(label, &noDraw)) FaceTexturing::SetFlag(*session, targets, FaceFlag::NoDraw, noDraw);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Face is not rendered, but still shapes the solid and its collision (clip, skip)");
    }

    // ---- Буфер ----
    if (ImGui::Button("Copy from first face")) FaceTexturing::Copy(*session, targets.front(), m_clipboard);
    ImGui::SameLine();
    ImGui::BeginDisabled(!m_clipboard.valid);
    if (ImGui::Button("Paste")) FaceTexturing::Paste(*session, targets, m_clipboard, m_pasteMaterial, m_pasteUV);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Checkbox("Material", &m_pasteMaterial);
    ImGui::SameLine();
    ImGui::Checkbox("UV", &m_pasteUV);
    if (m_clipboard.valid) ImGui::TextDisabled("Clipboard: %s", m_clipboard.materialName.c_str());

    // Выбор материала в сетке применяем здесь: SetMaterial может добавить материал в таблицу документа (реаллокация)
    if (!m_pendingMaterial.empty())
    {
        FaceTexturing::SetMaterial(*session, targets, m_pendingMaterial);
        m_pendingMaterial.clear();
    }

    ImGui::End();
}
