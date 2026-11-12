//
//  TexturingPanel.cpp
//  Cucumber
//

#include "TexturingPanel.h"

#include "MapComponent.h"
#include "SceneManager.h"
#include "TextureLibrary.h"
#include "UIManager.h"
#include "World.h"

#include <imgui.h>

#include <string>

namespace
{
    ImTextureID ToImGuiTexture(unsigned int handle)
    {
        return (ImTextureID)(intptr_t)handle;   // handle_t это имя GL-текстуры (так же во ViewportPanel)
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

    // Касается перемещения и дублирования солидов целиком, поэтому стоит выше выбора граней
    bool textureLock = session->textureLock();
    if (ImGui::Checkbox("Lock textures when moving solids", &textureLock)) session->setTextureLock(textureLock);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("On: texture moves together with the solid.\nOff: texture stays in place in the world.");

    const std::vector<FaceRef> targets = session->textureTargets();
    if (targets.empty())
    {
        ImGui::TextWrapped("Select a solid (click) or faces (Alt+click, Ctrl+Alt+click to add).");
        ImGui::End();
        return;
    }

    FaceTexturing::Summary summary;
    if (!FaceTexturing::Summarize(*session, targets, summary))
    {
        ImGui::End();
        return;
    }

    if (!session->selectedFaces().empty()) ImGui::Text("%d selected face(s)", summary.total);
    else                                   ImGui::Text("All faces of %d solid(s) (%d)", static_cast<int>(session->selectedSolids().size()), summary.total);
    ImGui::Separator();
    
    // ---- Флаги ----
    {
        const bool allNoDraw = summary.noDrawCount == summary.total;
        bool noDraw = allNoDraw;
        const char* label = (summary.noDrawCount > 0 && !allNoDraw) ? "No draw (some)" : "No draw";
        if (ImGui::Checkbox(label, &noDraw)) FaceTexturing::SetFlag(*session, targets, FaceFlag::NoDraw, noDraw);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Face is not rendered, but still shapes the solid and its collision (clip, skip)");
    }
    
    ImGui::Separator();

    // ---- Материал: только текущий, выбирать в TextureBrowserPanel ----
    LevelDocument& document = session->document();
    const std::string currentName = document.materials[summary.material];
    {
        const TextureLibrary::Info& info = TextureLibrary::Get().info(currentName);
        if (info.handle != 0) ImGui::Image(ToImGuiTexture(info.handle), ImVec2(64, 64));
        else                  ImGui::Dummy(ImVec2(64, 64));

        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted(summary.mixedMaterial ? "(several materials)" : currentName.c_str());
        if (!summary.mixedMaterial)
        {
            if (info.found) ImGui::TextDisabled("%d x %d", info.width, info.height);
            else            ImGui::TextDisabled("file not found");
        }
        if (ImGui::Button("Browse textures...") && m_manager) m_manager->showTextureBrowser();
        ImGui::EndGroup();
    }

    ImGui::Separator();

    // ---- Раскладка ----
    {
        glm::vec2 shift = summary.uv.shift;
        if (ImGui::DragFloat2(Label("Offset", summary.mixedShift).c_str(), &shift.x, 0.5f, 0.0f, 0.0f, "%.2f"))
            FaceTexturing::SetShift(*session, targets, shift, "tex.shift");
        if (ImGui::IsItemDeactivatedAfterEdit()) session->sealEdit();

        glm::vec2 scale = summary.uv.scale;
        if (ImGui::DragFloat2(Label("Scale", summary.mixedScale).c_str(), &scale.x, 0.01f, 0.0f, 0.0f, "%.3f"))
            FaceTexturing::SetScale(*session, targets, scale, "tex.scale");
        if (ImGui::IsItemDeactivatedAfterEdit()) session->sealEdit();

        float rotation = summary.rotation;
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
        FaceTexturing::FitToFace(*session, targets, [](const std::string& name)
        {
            const TextureLibrary::Info& info = TextureLibrary::Get().info(name);
            return glm::ivec2(info.width, info.height);
        }, m_repeat);
    }

    ImGui::Separator();

    // ---- Буфер ----
    if (ImGui::Button("Copy from first face")) FaceTexturing::Copy(*session, targets.front(), m_clipboard);
    ImGui::SameLine();
    ImGui::BeginDisabled(!m_clipboard.valid);
    if (ImGui::Button("Paste"))
    {
        FaceTexturing::Paste(*session, targets, m_clipboard, m_pasteMaterial, m_pasteUV,
                             m_pasteWrap ? FaceTexturing::UVMode::Wrap : FaceTexturing::UVMode::World);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Checkbox("Material", &m_pasteMaterial);
    ImGui::SameLine();
    ImGui::Checkbox("UV", &m_pasteUV);
    ImGui::BeginDisabled(!m_pasteUV);
    ImGui::Checkbox("Wrap around edges", &m_pasteWrap);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("On: the pattern continues across the shared edge (like Hammer's align-to-face).\n"
                          "Off: the same world-space projection is applied as is; fine for parallel faces,\n"
                          "but stretches the texture on faces turned away from the source.");
    if (m_clipboard.valid) ImGui::TextDisabled("Clipboard: %s", m_clipboard.materialName.c_str());

    ImGui::End();
}
