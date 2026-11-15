//
//  InspectorComponents.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 13.10.26.
//

#include "ComponentInspector.h"

#include "InspectorPanel.h"
#include "AssetsPanel.h"
#include "UIManager.h"
#include "SceneManager.h"

#include "TransformComponent.h"
#include "RigidbodyComponent.h"
#include "CollisionComponent.h"
#include "MovementComponent.h"
#include "AnimatorComponent.h"
#include "CameraComponent.h"
#include "ScriptComponent.h"
#include "AudioComponent.h"
#include "LightComponent.h"
#include "MeshComponent.h"
#include "MapComponent.h"
#include "ProjectManager.h"

#include "Mesh.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <cstring>

#include "../vendor/IconsMaterialDesignIcons.h"
#include "../vendor/tinyfiledialogs.h"

void ComponentInspector::Render(Component* component, InspectorPanel* panel)
{
    if (!component) return;
    
    std::type_index id = component->GetTypeIndex();
    auto it = registry.find(id);
    
    ImGui::PushID(component);
    
    if (it != registry.end()) {
        const auto& info = it->second;
        
        // Отрисовываем красивый заголовок в ImGui
        if (ImGui::CollapsingHeader(info.displayName.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            // Вызываем кастомную функцию отрисовки
            info.renderFunc(component, panel);
        }
    } else {
        // Фолбэк, если UI для компонента забыли зарегистрировать
        ImGui::CollapsingHeader(typeid(*component).name());
    }
    
    ImGui::PopID();
}

std::string ComponentInspector::GetName(std::type_index id) {
    auto it = registry.find(id);
    return (it != registry.end()) ? it->second.displayName : id.name();
}

static bool DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue,
                            float columnWidth, bool* linkedAxes = nullptr)
{
    ImGuiIO& io = ImGui::GetIO();
    auto boldFont = io.Fonts->Fonts[1];
    
    ImGui::PushID(label.c_str());
    
    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, columnWidth);
    ImGui::Text("%s", label.c_str());
    bool linkedAxesChanged = false;
    if (linkedAxes)
    {
        ImGui::SameLine();
        const bool wasLinked = *linkedAxes;
        if (wasLinked) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
        }
        const char* linkIcon = *linkedAxes ? ICON_MDI_LINK_VARIANT : ICON_MDI_LINK_VARIANT_OFF;
        if (ImGui::SmallButton(linkIcon)) {
            *linkedAxes = !*linkedAxes;
            linkedAxesChanged = true;
            if (*linkedAxes) values = glm::vec3(values.x);
        }
        if (wasLinked) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(*linkedAxes ? "Scale axes linked" : "Scale axes independent");
        }
    }
    ImGui::NextColumn();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });
    
    float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
    ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

    ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
    
    bool isChangedX = false;
    bool isChangedY = false;
    bool isChangedZ = false;
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.0f, 0.25f, 0.3f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.8f, 0.0f, 0.25f, 0.4f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.0f, 0.25f, 0.7f });
    
    ImGui::PushFont(boldFont);
    if (ImGui::Button("X", buttonSize)) {
        if (linkedAxes && *linkedAxes) values = glm::vec3(resetValue);
        else values.x = resetValue;
        isChangedX = true;
    }
    ImGui::PopFont();
    
    ImGui::PopStyleColor(3);
    
    ImGui::SameLine();
    if (ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f")) {
        if (linkedAxes && *linkedAxes) values = glm::vec3(values.x);
        isChangedX = true;
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.25f, 0.8f, 0.25f, 0.3f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.25f, 0.8f, 0.25f, 0.4f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.25f, 0.8f, 0.25f, 0.7f });
    
    ImGui::PushFont(boldFont);
    if (ImGui::Button("Y", buttonSize)) {
        if (linkedAxes && *linkedAxes) values = glm::vec3(resetValue);
        else values.y = resetValue;
        isChangedY = true;
    }
    ImGui::PopFont();
    
    ImGui::PopStyleColor(3);
    
    ImGui::SameLine();
    if (ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f")) {
        if (linkedAxes && *linkedAxes) values = glm::vec3(values.y);
        isChangedY = true;
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 0.3f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.1f, 0.25f, 0.8f, 0.4f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 0.7f });
    
    ImGui::PushFont(boldFont);
    if (ImGui::Button("Z", buttonSize)) {
        if (linkedAxes && *linkedAxes) values = glm::vec3(resetValue);
        else values.z = resetValue;
        isChangedZ = true;
    }
    ImGui::PopFont();
    
    ImGui::PopStyleColor(3);
    
    ImGui::SameLine();
    if (ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f")) {
        if (linkedAxes && *linkedAxes) values = glm::vec3(values.z);
        isChangedZ = true;
    }
    
    ImGui::PopItemWidth();
    
    ImGui::PopStyleVar();
    
    ImGui::Columns(1);
    
    ImGui::PopID();
    
    return linkedAxesChanged || isChangedX || isChangedY || isChangedZ;
}

static void DrawTransformComponent(TransformComponent* transform)
{
    static bool scaleAxesLinked = true;

    // 1. ПОЗИЦИЯ
    glm::vec3 pos = transform->getPosition();
    // Передаем копию. Если пользователь покрутил ползунок, функция вернет true
    if (DrawVec3Control("Position", pos, 0.0f, 100.0f)) {
        transform->setPosition(pos); // Срабатывает сеттер и инвалидация!
    }
    
    // 2. ПОВОРОТ (Скрываем кватернион, показываем углы Эйлера)
    // Извлекаем углы из трансформера (или используем сохраненный m_eulerDegrees, как обсуждали ранее)
    glm::vec3 eulerAngles = transform->getRotation();
    
    if (DrawVec3Control("Rotation", eulerAngles, 0.0f, 100.0f)) {
        transform->setRotation(eulerAngles); // Сеттер переведет в кватернион и сбросит кэш
    }
    
    // 3. МАСШТАБ
    glm::vec3 scale = transform->getScale();
    if (DrawVec3Control("Scale", scale, 1.0f, 100.0f, &scaleAxesLinked)) {
        transform->setScale(scale);
    }
}

static void DrawCollisionBits(const char* label, uint32_t& bitmask)
{
    ImGui::Text("%s", label);
    ImGui::PushID(label);
    
    // Сетка 4х4 для 16 слоев (если нужно 32, сделайте 8х4 или 4х8)
    const int numRows = 2;
    const int numCols = 8;
    
    // Делаем кнопки маленькими и квадратными
    ImVec2 buttonSize = ImVec2(25, 25);
    
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
    
    for (int row = 0; row < numRows; ++row)
    {
        for (int col = 0; col < numCols; ++col)
        {
            int bitIndex = row * numCols + col;
            uint32_t bitValue = (1 << bitIndex);
            bool isSet = (bitmask & bitValue) != 0;
            
            // Если бит активен — подсвечиваем кнопку
            if (isSet)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.33f, 0.18f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.05f, 0.18f, 0.10f, 1.0f));
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.05f, 0.05f, 0.05f, 1.0f));
            }
            
            // Название кнопки — просто индекс бита (от 1 до 16 для удобства человека)
            char buf[4];
            snprintf(buf, sizeof(buf), "%d", bitIndex);
            
            ImGui::PushID(bitIndex);
            if (ImGui::Button(buf, buttonSize))
            {
                if (isSet) {
                    bitmask &= ~bitValue; // Выключаем бит
                } else {
                    bitmask |= bitValue;  // Включаем бит
                }
            }
            
            // Всплывающая подсказка при наведении на бит
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Bit %d (Value: %u)", bitIndex, bitValue);
            }
            ImGui::PopID();
            
            ImGui::PopStyleColor(3);
            
            // Если это не последняя колонка — рисуем на той же строке
            if (col < numCols - 1) {
                ImGui::SameLine();
            }
        }
    }
    
    ImGui::PopStyleVar();
    ImGui::PopID();
    ImGui::Spacing();
}

static void DrawMeshComponent(MeshComponent* comp)
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Source");
    ImGui::SameLine();
    
    char pathBuffer[256];
    std::snprintf(pathBuffer, sizeof(pathBuffer), "%s", comp->path.c_str());
    ImGui::InputText("##Path", pathBuffer, sizeof(pathBuffer), ImGuiInputTextFlags_ReadOnly);
    
    ImGui::PushStyleColor(ImGuiCol_DragDropTarget, IM_COL32(0, 0, 0, 0));
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_ITEM", ImGuiDragDropFlags_AcceptBeforeDelivery))
        {
            AssetDragPayload* info = static_cast<AssetDragPayload*>(payload->Data);
            
            const bool isModel = info->type == AssetType_Mesh;
            
            const ImVec2 fieldMin = ImGui::GetItemRectMin();
            const ImVec2 fieldMax = ImGui::GetItemRectMax();
            const ImU32 color = isModel ? IM_COL32(80, 190, 110, 230) : IM_COL32(220, 100, 70, 230);
            ImGui::GetWindowDrawList()->AddRect(fieldMin, fieldMax, color, 0.0f, 0, 1.0f);

            if (payload->IsDelivery() && isModel)
            {
                comp->loadModel(info->path);
            }
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::PopStyleColor();
    
    if (comp->mesh) {
        ImGui::Checkbox("Skybox ambient", &comp->mesh->m_useAmbientLight);
        DrawCollisionBits("Layer", comp->layer);
    }
    
    if (comp->mesh && !comp->mesh->m_surfaces.empty())
    {
        if (ImGui::TreeNode("Surfaces"))
        {
            for (int i = 0; i < comp->mesh->m_surfaces.size(); i++)
            {
                if (ImGui::TreeNode((void *)(intptr_t)i, "Surface %i", i))
                {
                    auto &surface = comp->mesh->m_surfaces[i];
                    ImGui::Text("%s: %i tris, %i material", surface.m_name.c_str(),
                                surface.count / 3, surface.materialSlot);
                    
                    ImGui::TreePop();
                }
            }
            
            ImGui::TreePop();
        }
    }
}

static void DrawMapComponent(MapComponent* comp)
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Source");
    ImGui::SameLine();
    
    char pathBuffer[256];
    std::snprintf(pathBuffer, sizeof(pathBuffer), "%s", comp->path.c_str());
    ImGui::InputText("##Path", pathBuffer, sizeof(pathBuffer), ImGuiInputTextFlags_ReadOnly);
    
    ImGui::PushStyleColor(ImGuiCol_DragDropTarget, IM_COL32(0, 0, 0, 0));
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_ITEM", ImGuiDragDropFlags_AcceptBeforeDelivery))
        {
            AssetDragPayload* info = static_cast<AssetDragPayload*>(payload->Data);
            
            const bool isModel = info->type == AssetType_Map;
            
            const ImVec2 fieldMin = ImGui::GetItemRectMin();
            const ImVec2 fieldMax = ImGui::GetItemRectMax();
            const ImU32 color = isModel ? IM_COL32(80, 190, 110, 230) : IM_COL32(220, 100, 70, 230);
            ImGui::GetWindowDrawList()->AddRect(fieldMin, fieldMax, color, 0.0f, 0, 1.0f);
            
            if (payload->IsDelivery() && isModel)
            {
                comp->importTrenchBroomMap(info->path, true);
            }
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::PopStyleColor();
    
    if (comp->isEditing())
    {
        if (ImGui::Button("Done")) comp->endEdit();
        ImGui::SameLine();
        if (ImGui::Button("Save")) comp->saveLevel();
        if (comp->isLevelDirty()) ImGui::TextColored(ImVec4(1, 0.7f, 0.2f, 1), "Unsaved changes");
        return;   // Reload / Bake / Rebuild UVs в режиме правки недоступны
    }
    if (ImGui::Button("Edit")) comp->beginEdit();
    ImGui::SameLine();
    
    if (ImGui::Button("Reimport"))
    {
        comp->reimportSource();
    }
    
    ImGui::SameLine();

    if (comp->isBaking())
    {
        ImGui::ProgressBar(comp->bakeProgress());
        if (ImGui::Button("Cancel")) comp->cancelBake();
    }
    else if (ImGui::Button("Bake lighting"))
    {
        comp->bakeLighting();
    }
    
    if (comp->isLightmapOutdated())
    {
        ImGui::TextColored(ImVec4(1, 0.7f, 0.2f, 1), "Lightmap is outdated");
    }
    
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Lightmap density");
    ImGui::SameLine();
    ImGui::DragFloat("##lightmap_density", &comp->lightmapTexelsPerUnit, 0.01f, 0.01f, 16.0f, "%.4f");
    ImGui::TextDisabled("Rebuild UVs after changing this value.");
    
    if (ImGui::Button("Rebuild Lightmap UVs"))
    {
        comp->rebuildLightmapUVs();
    }
    
    if (!comp->bakeStatus.empty())
    {
        ImGui::TextUnformatted(comp->bakeStatus.c_str());
    }
}

static void DrawCollisionComponent(CollisionComponent* comp)
{
    //    ImGui::DragFloat3("Mins", &comp->m_mins.x, 1.f);
    //    ImGui::DragFloat3("Maxs", &comp->m_maxs.x, 1.f);
    
    DrawVec3Control("Min Bounds", comp->m_mins, -16.0f, 100.0f);
    DrawVec3Control("Max Bounds", comp->m_maxs,  16.0f, 100.0f);
    
    ImGui::Separator();
    
    DrawCollisionBits("Layer", comp->layer);
    //    DrawCollisionBits("Mask (С чем сталкивается)", comp->collisionMask);
    
    ImGui::Separator();
    
    ImGui::Checkbox("Trigger", &comp->isTrigger);
}

static void DrawRigidbodyComponent(RigidbodyComponent* comp)
{
    //    ImGui::DragFloat3("Mins", &comp->m_mins.x, 1.f);
    //    ImGui::DragFloat3("Maxs", &comp->m_maxs.x, 1.f);
    
    DrawVec3Control("Half Extents", comp->boxHalfExtents, -16.0f, 100.0f);
}

static void DrawAudioDetails(AudioComponent* component)
{
    // 1. Управление воспроизведением (Кнопки Play / Pause / Stop в один ряд)
    if (component->IsPlaying()) {
        // Если играет, показываем кнопку Pause (в miniaudio это просто Stop)
        if (ImGui::Button("Pause##SoundPause")) {
            component->stop();
        }
    } else {
        if (ImGui::Button("Play##SoundPlay")) {
            component->play();
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Stop##SoundStop")) {
        component->stop();
        component->SetCurrentTime(0.0f); // Сбрасываем в начало
    }
    
    ImGui::SameLine();
    bool isLooping = component->IsLooping();
    if (ImGui::Checkbox("Looping##SoundLooping", &isLooping)) {
        component->setLooping(isLooping);
    }
    
    //    // 2. Прогресс-бар воспроизведения
    float currentTime = component->GetCurrentTime();
    float totalTime = component->m_length;
    float progress = (totalTime > 0.0f) ? (currentTime / totalTime) : 0.0f;
    
    // Форматируем текст времени: "01:23 / 03:45"
    char timeBuffer[64];
    snprintf(timeBuffer, sizeof(timeBuffer), "%.1fs / %.1fs", currentTime, totalTime);
    
    // Отрисовка прогресс-бара
    ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f), timeBuffer);
    
    //    // Интерактивная перемотка (Slider), если хочется кликать по таймлайну
    //    float seekTime = currentTime;
    //    if (ImGui::SliderFloat("Seek", &seekTime, 0.0f, totalTime, " ")) {
    //        component->SetCurrentTime(seekTime);
    //    }
    //
    //    // 3. Основные настройки звука
    //    // Настройки громкости
    //    float volume = component->GetVolume();
    //    if (ImGui::SliderFloat("Volume", &volume, 0.0f, 2.0f, "%.2f")) {
    //        component->setVolume(volume);
    //    }
}

static void DrawAudioComponent(AudioComponent* comp)
{
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s", comp->path.c_str());
    
    ImGui::InputText("##Path", buffer, 256, ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (ImGui::Button("..."))
    {
        char const * filter[2] = { "*.mp3", "*.wav" };
        const auto soundsPath = ProjectManager::Get().assetsPath() / "sounds";
        const char* path = tinyfd_openFileDialog(NULL, soundsPath.string().c_str(), 2, filter, NULL, 0);
        
        if (path != nullptr)
        {
            comp->init(path);
            
            const auto base_path = ProjectManager::Get().assetsPath();
            auto rel_path = std::filesystem::relative(path, base_path);
            comp->path = rel_path.string();
        }
    }
    
    if (comp->m_isInitialized)
    {
        DrawAudioDetails(comp);
    }
}

static void DrawLightComponent(LightComponent *comp)
{
    static constexpr const char* lightTypeNames[] = {
        "Directional",
        "Point",
        "Spot"
    };

    int type = static_cast<int>(comp->type);
    if (ImGui::Combo("Type", &type, lightTypeNames, IM_ARRAYSIZE(lightTypeNames)))
    {
        comp->type = static_cast<LightType>(type);
    }

    static constexpr const char* lightModeNames[] = {
        "Realtime",
        "Baked",
        "Baked + Realtime"
    };
    int mode = static_cast<int>(comp->mode);
    if (ImGui::Combo("Mode", &mode, lightModeNames, IM_ARRAYSIZE(lightModeNames)))
    {
        comp->mode = static_cast<LightMode>(mode);
    }

    ImGui::ColorEdit3("Color", &comp->color.x);
    ImGui::DragFloat("Intensity", &comp->intensity, 0.1f, 0.0f, 100000.0f, "%.2f");
    ImGui::Checkbox("Cast Shadows", &comp->castShadows);

    if (comp->type != LightType::Directional)
    {
        ImGui::DragFloat("Range", &comp->range, 1.0f, 0.01f, 100000.0f, "%.2f");
        ImGui::DragFloat("Linear Attenuation", &comp->linear, 0.001f, 0.0f, 1000.0f, "%.4f");
        ImGui::DragFloat("Quadratic Attenuation", &comp->quadratic, 0.001f, 0.0f, 1000.0f, "%.4f");
    }
    if (comp->type == LightType::Spot)
    {
        ImGui::SliderFloat("Spot Angle", &comp->spotAngle, 1.0f, 89.0f, "%.1f deg");
    }
}

static void DrawScriptProperties(ScriptComponent* component)
{
    if (!component || !ImGui::TreeNode("Variables")) return;

    const auto& properties = component->getScriptProperties();
    if (ImGui::BeginTable("ScriptProperties", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, 0.4f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.6f);

        for (const ScriptPropertyDescriptor& property : properties)
        {
            if (property.hidden || property.kind == ScriptPropertyKind::Unsupported) continue;

            if (!property.header.empty())
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "%s",
                                   property.header.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::Separator();
            }

            nlohmann::json value = component->getPropertyValue(property.name);
            if (value.is_null() && property.kind != ScriptPropertyKind::Reference) continue;

            ImGui::PushID(property.name.c_str());
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(property.name.c_str());
            ImGui::TableSetColumnIndex(1);

            switch (property.kind)
            {
                case ScriptPropertyKind::Float:
                {
                    float current = value.get<float>();
                    const bool changed = property.hasRange
                        ? ImGui::SliderFloat("##value", &current, property.rangeMin, property.rangeMax)
                        : ImGui::DragFloat("##value", &current, 0.05f);
                    if (changed) component->setPropertyValue(property.name, current);
                    break;
                }
                case ScriptPropertyKind::Integer:
                {
                    int current = value.get<int>();
                    const bool changed = property.hasRange
                        ? ImGui::SliderInt("##value", &current,
                                           static_cast<int>(property.rangeMin),
                                           static_cast<int>(property.rangeMax))
                        : ImGui::DragInt("##value", &current);
                    if (changed) component->setPropertyValue(property.name, current);
                    break;
                }
                case ScriptPropertyKind::Boolean:
                {
                    bool current = value.get<bool>();
                    if (ImGui::Checkbox("##value", &current))
                        component->setPropertyValue(property.name, current);
                    break;
                }
                case ScriptPropertyKind::String:
                {
                    std::string current = value.get<std::string>();
                    char buffer[512] = {};
                    std::strncpy(buffer, current.c_str(), sizeof(buffer) - 1);
                    if (ImGui::InputText("##value", buffer, sizeof(buffer)))
                        component->setPropertyValue(property.name, std::string(buffer));
                    break;
                }
                case ScriptPropertyKind::Reference:
                {
                    const bool hasReference = value.is_object();
                    const std::string buttonLabel = hasReference
                        ? value.value("display", "Linked " + property.typeName)
                        : "None (" + property.typeName + ")";
                    ImGui::PushStyleColor(ImGuiCol_Button,
                                          hasReference ? ImVec4(0.2f, 0.4f, 0.2f, 1.0f)
                                                       : ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                    ImGui::Button((buttonLabel + "##value").c_str(),
                                  ImVec2(ImGui::GetContentRegionAvail().x, 0));
                    ImGui::PopStyleColor();

                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload =
                                ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY"))
                        {
                            Entity* target = *static_cast<Entity**>(payload->Data);
                            component->setPropertyReference(property.name, target);
                        }
                        ImGui::EndDragDropTarget();
                    }
                    if (ImGui::BeginPopupContextItem("script_reference_context"))
                    {
                        if (ImGui::MenuItem("Clear Reference"))
                            component->clearPropertyReference(property.name);
                        ImGui::EndPopup();
                    }
                    break;
                }
                case ScriptPropertyKind::Unsupported:
                    break;
            }

            if (!property.tooltip.empty() && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", property.tooltip.c_str());
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::TreePop();
}

void RegisterComponentsUI() {
    auto& inspector = ComponentInspector::Get();
    
    inspector.Register<TransformComponent>("Transform", [](Component* c, InspectorPanel* p) {
        auto* comp = static_cast<TransformComponent*>(c);
        DrawTransformComponent(comp);
    });
    
    inspector.Register<MeshComponent>("Mesh", [](Component* c, InspectorPanel* p) {
        auto* comp = static_cast<MeshComponent*>(c);
        DrawMeshComponent(comp);
    });
    
    inspector.Register<MapComponent>("World Static", [](Component* c, InspectorPanel* p) {
        auto* comp = static_cast<MapComponent*>(c);
        DrawMapComponent(comp);
    });
    
    inspector.Register<AnimatorComponent>("Animator", [](Component* c, InspectorPanel* p) {
        auto* comp = static_cast<AnimatorComponent*>(c);
        if (!comp->m_clips.empty())
        {
            auto& clips = comp->m_clips;
            
            const char* title = comp->m_currentClipIndex == -1 ? "<no selected>" : clips[comp->m_currentClipIndex]->name.c_str();
            
            if (ImGui::BeginCombo("##sequence combo", title, ImGuiComboFlags_None))
            {
                for (int i = 0; i < clips.size(); ++i)
                {
                    bool is_selected = (comp->m_currentClipIndex == i);
                    
                    if (ImGui::Selectable(clips[i]->name.c_str(), is_selected))
                    {
                        comp->play(i);
                    }
                    
                    if (is_selected) ImGui::SetItemDefaultFocus();
                }
                
                ImGui::EndCombo();
            }
        }
    });
    
    inspector.Register<AudioComponent>("Sound", [](Component* c, InspectorPanel* p) {
        auto* comp = static_cast<AudioComponent*>(c);
        DrawAudioComponent(comp);
    });
    
    inspector.Register<ScriptComponent>("Script", [](Component* c, InspectorPanel* p) {
        auto* comp = static_cast<ScriptComponent*>(c);
        
        ImGui::TextDisabled("%s", comp->scriptPath.c_str());
        
        if (comp->m_isInitialized)
        {
            if (ImGui::Button("Edit")) {
                
#if defined(_WIN32)
                // Для Windows: открывает файл в программе по умолчанию (обычно Блокнот)
                std::string command = "start \"" + comp->scriptPath + "\"";
#elif defined(__APPLE__)
                // Для macOS
                std::string command = "open \"" + comp->scriptPath + "\"";
#else
                // Для Linux
                std::string command = "xdg-open \"" + comp->scriptPath + "\"";
#endif
                
                std::system(command.c_str());
            }
            
            DrawScriptProperties(comp);
        }
    });
    
    inspector.Register<MovementComponent>("Movement", [](Component* c, InspectorPanel* p) {
        auto* comp = static_cast<MovementComponent*>(c);
        ImGui::DragFloat("Walk Speed##Movement", &comp->m_forwardspeed, 10.f, 0.0f, 1000.0f, "%.0f");
    });
    
    inspector.Register<CollisionComponent>("Collision", [](Component* c, InspectorPanel* p) {
        auto* comp = static_cast<CollisionComponent*>(c);
        DrawCollisionComponent(comp);
    });
    
    inspector.Register<RigidbodyComponent>("Rigidbody", [](Component* c, InspectorPanel* p) {
        auto* comp = static_cast<RigidbodyComponent*>(c);
        DrawRigidbodyComponent(comp);
    });

    inspector.Register<LightComponent>("Light", [](Component *c, InspectorPanel *p) {
          auto *comp = static_cast<LightComponent *>(c);
          DrawLightComponent(comp);
    });

    inspector.Register<CameraComponent>("Camera", [](Component* c, InspectorPanel* p) {
        auto* comp = static_cast<CameraComponent*>(c);
        ImGui::Checkbox("Enabled##Camera", &comp->enabled);
        ImGui::DragFloat("Field of view##Camera", &comp->fov, 10.f, 30.0f, 100.0f, "%.0f");
        ImGui::InputScalar("Camera Stack##Camera", ImGuiDataType_U32, &comp->cameraStackId);
        ImGui::Checkbox("Overlay Camera", &comp->m_isOverlay);
        if (!comp->m_isOverlay)
        {
            if (ImGui::Button("Make Primary##Camera"))
            {
                if (auto* scene = SceneManager::Get().getActiveScene())
                {
                    scene->setPrimaryCamera(comp);
                }
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(comp->m_isPrimary ? "Primary" : "Secondary");
        }
        DrawCollisionBits("Culling Mask", comp->cullingMask);
        ImGui::Checkbox("Clear Depth Only", &comp->clearDepthOnly);
        ImGui::DragFloat("Depth##Camera", &comp->depth, 1.f, 0.0f, 100.0f, "%.0f");
    });
}
