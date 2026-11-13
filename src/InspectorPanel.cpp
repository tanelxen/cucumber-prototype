//
//  ViewportPanel.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#include "InspectorPanel.h"
#include "ComponentInspector.h"
#include "AssetsPanel.h"
#include "UIManager.h"

#include "Entity.h"
#include "World.h"
#include "SceneManager.h"
#include "CollisionComponent.h"
#include "MovementComponent.h"
#include "CameraComponent.h"
#include "ScriptComponent.h"
#include "AudioComponent.h"
#include "LightComponent.h"
#include "MeshComponent.h"
#include "MapComponent.h"

#include "ScriptRuntime.h"

#include <imgui.h>

#include "../vendor/IconsMaterialDesignIcons.h"

#include <filesystem>

static bool s_isHovered;
static void DrawPrefabInterfaceEditor(Entity* prefabRoot, const std::string& prefabPath);

InspectorPanel::InspectorPanel() : EditorPanel(ICON_MDI_INFORMATION " Properties###Inspector")
{
    RegisterComponentsUI();
}

void InspectorPanel::setEntity(Entity *entity)
{
    m_entity = entity;
}

void InspectorPanel::draw()
{
    ImGui::Begin(m_name.c_str());
    
    s_isHovered = ImGui::IsItemHovered();

    WorldScene* activeScene = SceneManager::Get().getActiveScene();
    if (activeScene && activeScene->isPrefabContext())
    {
        Entity* prefabRoot = nullptr;
        for (const auto& entity : activeScene->m_entities)
        {
            if (entity && !entity->getParent()) {
                prefabRoot = entity.get();
                break;
            }
        }

        if (prefabRoot)
        {
            DrawPrefabInterfaceEditor(prefabRoot, activeScene->m_filePath);
            ImGui::Separator();
        }
    }
    
    if (!m_entity)
    {
        ImGui::End();
        return;
    }
    
    DrawInspector(m_entity);
    
    ImGui::End();
}

// Функция возвращает имена всех скриптовых классов, наследующих EntityScript
std::vector<std::string> GetAvailableScriptClasses()
{
    return GetScriptRuntime().getAvailableScriptClasses();
}

InspectorPanel::AssetDragResult InspectorPanel::RenderAssetDropZone(const char* str_id, const ImVec2& size, float targetLineY, uint32_t allowedTypes)
{
    ImGui::InvisibleButton(str_id, size);
    
    bool isHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    AssetDragResult result; // По умолчанию success = false
    
    // Предварительная проверка типа
    bool isValidAssetDragging = false;
    if (const ImGuiPayload* currentPayload = ImGui::GetDragDropPayload())
    {
        if (currentPayload->IsDataType("ASSET_ITEM"))
        {
            AssetDragPayload* dragData = (AssetDragPayload*)currentPayload->Data;
            if (dragData != nullptr && (dragData->type & allowedTypes))
            {
                isValidAssetDragging = true;
            }
        }
    }
    
    ImGui::PushStyleColor(ImGuiCol_DragDropTarget, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_NavHighlight,   IM_COL32(0, 0, 0, 0));
    
    if (ImGui::BeginDragDropTarget())
    {
        if (isValidAssetDragging)
        {
            if (const ImGuiPayload* imGuiPayload = ImGui::AcceptDragDropPayload("ASSET_ITEM"))
            {
                // КЛЮЧЕВОЙ МОМЕНТ: Копируем данные по значению, пока они валидны!
                result.payload = *(AssetDragPayload*)imGuiPayload->Data;
                result.success = true;
            }
        }
        ImGui::EndDragDropTarget();
    }
    
    ImGui::PopStyleColor(2);
    
    // Отрисовка линии
    if (isValidAssetDragging && isHovered)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        float startX = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMin().x;
        float endX   = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
        
        ImVec2 lineStart = ImVec2(startX, targetLineY);
        ImVec2 lineEnd   = ImVec2(endX, targetLineY + 4.0f);
        
        drawList->AddRectFilled(lineStart, lineEnd, IM_COL32(66, 150, 250, 255));
    }
    
    // Теперь данные внутри result.payload в полной безопасности, так как они скопированы в стек
    return result;
}

void InspectorPanel::DrawNormalEntityComponents(Entity* entity)
{
    ImGui::Checkbox("##active", &m_entity->m_isActive);
    
    ImGui::SameLine();
    
    char buffer[256] = "";
    snprintf(buffer, sizeof(buffer), "%s", m_entity->m_name.c_str());
    
    if (ImGui::InputText("##Name", buffer, sizeof(buffer)))
    {
        auto& name = m_entity->m_name;
        name = buffer;
    }
    
    ImGui::TextDisabled("%s", m_entity->uid.toText().c_str());
    
    ImGui::Separator();
    
    ComponentInspector::Get().Render(&m_entity->getTransform(), this);
    ImGui::Spacing();
    
    std::vector<Component*> components;
    components.reserve(m_entity->getComponents().size());
    for (const auto& component : m_entity->getComponents())
    {
        components.push_back(component.get());
    }

    for (Component* component : components)
    {
        ComponentInspector::Get().Render(component, this);
        ImGui::Spacing();
    }
    
    uint32_t allowedInInspector = AssetType_Mesh | AssetType_Map;
    
    // ЗОНА 1: Между последним компонентом и кнопкой "Add Component"
    float targetLineY = ImGui::GetCursorScreenPos().y;
    auto result1 = RenderAssetDropZone("##drop_before_btn", ImVec2(-1, 6.0f), targetLineY, allowedInInspector);
    if (result1.success)
    {
        ProcessAssetInInspector(&result1.payload); // Передаем адрес скопированных данных
    }

    ImGui::Separator();
    ImGui::Spacing();
    
    if (ImGui::Button("Add Component"))
    {
        ImGui::OpenPopup("AddComponentPopup");
    }
    
    // ЗОНА 2: Пустое пространство в самом низу панели
    ImVec2 remainingSpace = ImGui::GetContentRegionAvail();
    float bottomZoneHeight = std::max(remainingSpace.y, 20.0f);
    auto result2 = RenderAssetDropZone("##drop_bottom", ImVec2(-1, bottomZoneHeight), targetLineY, allowedInInspector);
    if (result2.success)
    {
        ProcessAssetInInspector(&result2.payload);
    }
    
    if (ImGui::BeginPopup("AddComponentPopup"))
    {
        if (ImGui::MenuItem("Mesh"))            { m_entity->addComponent<MeshComponent>(); }
        if (ImGui::MenuItem("Sound"))           { m_entity->addComponent<AudioComponent>(); }
        if (ImGui::MenuItem("Light"))           { m_entity->addComponent<LightComponent>(); }
        if (ImGui::MenuItem("Camera"))          { m_entity->addComponent<CameraComponent>(); }
        if (ImGui::MenuItem("Movement"))        { m_entity->addComponent<MovementComponent>(); }
        if (ImGui::MenuItem("Collision"))       { m_entity->addComponent<CollisionComponent>(); }
        if (ImGui::MenuItem("World Static"))    { m_entity->addComponent<MapComponent>(); }
        
        if (ImGui::BeginMenu("Scripts"))
        {
            // Получаем актуальный список классов из AngelScript
            auto availableScripts = GetAvailableScriptClasses();
            
            if (availableScripts.empty())
            {
                ImGui::TextDisabled("No scripts found");
            }
            
            for (const auto& className : availableScripts)
            {
                if (ImGui::MenuItem(className.c_str()))
                {
                    auto comp = m_entity->addComponent<ScriptComponent>();
                    comp->init(className);
                    
                    ImGui::CloseCurrentPopup();
                }
            }
            
            ImGui::EndMenu();
        }
        
        ImGui::EndPopup();
    }
}

void InspectorPanel::ProcessAssetInInspector(AssetDragPayload* data)
{
    if (data->type == AssetType_Mesh)
    {
        auto meshComponent = m_entity->addComponent<MeshComponent>();
        meshComponent->loadModel(data->path);
    }
    if (data->type == AssetType_Map)
    {
        auto mapComponent = m_entity->addComponent<MapComponent>();
        mapComponent->importTrenchBroomMap(data->path, true);
    }
    else if (data->type == AssetType_Audio)
    {
        
    }
}

// Универсальный метод отрисовки поля ввода в зависимости от содержимого JSON
static bool DrawExposedWidget(const std::string& label, nlohmann::json& ioValue)
{
    bool changed = false;
    
    // 1. Если это число с плавающей точкой (float)
    if (ioValue.is_number_float())
    {
        float val = ioValue.get<float>();
        if (ImGui::DragFloat(label.c_str(), &val, 0.1f)) {
            ioValue = val;
            changed = true;
        }
    }
    // 2. Если это целое число (int)
    else if (ioValue.is_number_integer())
    {
        int val = ioValue.get<int>();
        if (ImGui::DragInt(label.c_str(), &val, 1)) {
            ioValue = val;
            changed = true;
        }
    }
    // 3. Если это логический тип (bool / галочка)
    else if (ioValue.is_boolean())
    {
        bool val = ioValue.get<bool>();
        if (ImGui::Checkbox(label.c_str(), &val)) {
            ioValue = val;
            changed = true;
        }
    }
    // 4. Если это строка (std::string / ввод текста)
    else if (ioValue.is_string())
    {
        std::string val = ioValue.get<std::string>();
        char buf[256];
        strncpy(buf, val.c_str(), sizeof(buf));
        if (ImGui::InputText(label.c_str(), buf, sizeof(buf))) {
            ioValue = std::string(buf);
            changed = true;
        }
    }
    // 5. Если это массив из 3-х чисел (например, glm::vec3 для Position/Rotation)
    else if (ioValue.is_array() && ioValue.size() == 3 && ioValue[0].is_number())
    {
        float vec[3] = { ioValue[0].get<float>(), ioValue[1].get<float>(), ioValue[2].get<float>() };
        if (ImGui::DragFloat3(label.c_str(), vec, 0.1f)) {
            ioValue = { vec[0], vec[1], vec[2] };
            changed = true;
        }
    }
    
    return changed;
}

void InspectorPanel::DrawPrefabInstanceInspector(Entity* prefabRoot)
{
    if (!prefabRoot || !prefabRoot->isPrefabRoot) return;
    
    //    ImGui::Text("Prefab Instance: %s", prefabRoot->getName().c_str());
    ImGui::TextDisabled("Source: %s", prefabRoot->prefabAssetPath.c_str());
    ImGui::Separator();
    ImGui::Spacing();

    ComponentInspector::Get().Render(&prefabRoot->getTransform(), this);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::Text("Prefab Properties");
    ImGui::Spacing();

    if (prefabRoot->m_exposedInterfaceCache.empty()) {
        ImGui::TextDisabled("This prefab has no public properties.");
    }
    
    // Запускаем плоский цикл по нашему оптимизированному кэшу указателей!
    for (auto& prop : prefabRoot->m_exposedInterfaceCache)
    {
        if (!prop.targetComponent) continue;
        
        // 1. Получаем текущее геймплейное значение переменной из компонента
        nlohmann::json currentValue = prop.targetComponent->getExposedValue(prop.propertyName);
        
        // 2. Проверяем статус оверрайда на сцене для визуального выделения
        bool isOverridden = prefabRoot->isInterfaceOverrideDirty(prop.displayName);
        
        // 3. Если значение изменено на сцене — включаем жирный шрифт (стиль Unity)
        if (isOverridden)
        {
            // Предполагается, что у вас загружен жирный шрифт в ImGui, например, вторым в списке
            // Если шрифта нет, можно просто подсветить текст цветом (ImGui::PushStyleColor)
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
        }
        
        // Строим уникальный ID виджета для ImGui, чтобы избежать конфликтов имен
        std::string widgetLabel = "##" + prop.displayName;
        
        // Рисуем имя свойства слева, сам виджет справа
        ImGui::Columns(2, nullptr, false);
        ImGui::SetColumnWidth(0, 150.0f); // Фиксированная ширина под названия свойств
        
//        if (isOverridden) {
//            // Подсвечиваем измененный текст светло-зеленым/голубым цветом
//            ImGui::TextColored(ImVec4(0.3f, 0.85f, 0.3f, 1.0f), "%s *", prop.displayName.c_str());
//        } else {
            ImGui::Text("%s", prop.displayName.c_str());
//        }
        
        ImGui::NextColumn();
        ImGui::PushItemWidth(-1.0f); // Растянуть виджет на всю ширину колонки
        
        // 4. Отрисовываем сам элемент ввода с помощью нашего хелпера
        if (DrawExposedWidget(widgetLabel, currentValue))
        {
            // Пользователь изменил значение: пушим его обратно напрямую в память компонента по указателю!
            prop.targetComponent->setExposedValue(prop.propertyName, currentValue);
            
            // Маркируем для сцены, что этот оверрайд активен
            prefabRoot->markInterfaceOverrideDirty(prop.displayName, true);
        }
        
        ImGui::PopItemWidth();
        
        // 5. Контекстное меню по правой кнопке мыши (Сброс значения / Revert)
        if (isOverridden)
        {
            ImGui::PopFont(); // Отключаем жирный шрифт перед отрисовкой меню
            
//            // Если кликнули правой кнопкой по свойству
//            if (ImGui::BeginPopupContextItem(prop.displayName.c_str()))
//            {
//                if (ImGui::MenuItem("Revert to Prefab Default"))
//                {
//                    // Читаем дефолтное значение из исходного файла префаба
//                    // (Для максимальной скорости можно кэшировать и дефолты, но для UI считывание раз в клик не критично)
//                    nlohmann::json prefabJson;
//                    std::ifstream file(prefabRoot->prefabAssetPath);
//                    file >> prefabJson;
//
//                    // Находим базовое значение сущности, у которой parent == "" (это корень префаба)
//                    nlohmann::json entities = prefabJson.is_array() ? prefabJson : prefabJson["entities"];
//                    nlohmann::json rootEntityJson;
//                    for (const auto& ent : entities) {
//                        if (!ent.contains("parent") || ent["parent"].get<std::string>().empty() || ent["parent"] == "") {
//                            rootEntityJson = ent;
//                            break;
//                        }
//                    }
//
//                    // Ищем нужный компонент в исходном файле префаба по типу
//                    nlohmann::json defaultVal;
//                    if (rootEntityJson.contains("components")) {
//                        for (const auto& comp : rootEntityJson["components"]) {
//                            if (comp["type"] == prop.targetComponent->getTypeName()) {
//                                if (comp["data"].contains(prop.propertyName)) {
//                                    defaultVal = comp["data"][prop.propertyName];
//                                }
//                                break;
//                            }
//                        }
//                    }
//
//                    // Если нашли дефолтное значение — возвращаем его
//                    if (!defaultVal.empty()) {
//                        prop.targetComponent->setExposedValue(prop.propertyName, defaultVal);
//                        prefabRoot->markInterfaceOverrideDirty(prop.displayName, false); // Снимаем флаг оверрайда!
//                    }
//                }
//                ImGui::EndPopup();
//            }
        }
        else if (isOverridden)
        {
            ImGui::PopFont(); // Обязательный баланс PopFont, если меню не открывалось
        }
        
        ImGui::Columns(1); // Сброс колонок для следующей строки
        ImGui::Spacing();
    }
}

static void DrawPrefabInterfaceEditor(Entity* prefabRoot, const std::string& prefabPath)
{
    if (ImGui::CollapsingHeader("Prefab Interface", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextDisabled("Public properties available on prefab instances.");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Source: %s", prefabPath.c_str());
        }
        if (ImGui::Button("Save Prefab"))
        {
            SceneManager::Get().saveCurrent();
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Expose Property"))
        {
            CachedExposedProperty newProp;
            newProp.displayName = "New Property";
            prefabRoot->m_exposedInterfaceCache.push_back(newProp);
        }
        ImGui::Separator();

        if (prefabRoot->m_exposedInterfaceCache.empty()) {
            ImGui::TextDisabled("No public properties. Use + Expose Property to add one.");
        }

        if (ImGui::BeginTable("InterfaceTable", 3,
                              ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("##display_name", ImGuiTableColumnFlags_WidthStretch, 0.38f);
            ImGui::TableSetupColumn("##target_property", ImGuiTableColumnFlags_WidthStretch, 0.54f);
            ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed, 26.0f);

            int propIdx = 0;
            for (auto it = prefabRoot->m_exposedInterfaceCache.begin();
                 it != prefabRoot->m_exposedInterfaceCache.end();)
            {
                ImGui::TableNextRow();
                ImGui::PushID(propIdx);

                ImGui::TableSetColumnIndex(0);
                char nameBuf[64]{};
                std::snprintf(nameBuf, sizeof(nameBuf), "%s", it->displayName.c_str());
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::InputText("##display_name", nameBuf, sizeof(nameBuf))) {
                    it->displayName = nameBuf;
                }

                ImGui::TableSetColumnIndex(1);
                std::string propertyButton = "Choose target...";
                std::string targetTooltip;
                if (it->targetComponent && it->targetComponent->owner) {
                    const std::string path =
                        it->targetComponent->owner->getPathRelativeTo(prefabRoot);
                    const std::string targetName = path.empty() ? "Root" : path;
                    propertyButton = targetName + " / " + it->propertyName;
                    targetTooltip = targetName + " / " +
                                    it->targetComponent->getTypeName() + "::" +
                                    it->propertyName;
                }
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::Button(propertyButton.c_str(), ImVec2(-1.0f, 0.0f))) {
                    ImGui::OpenPopup("SelectPrefabProperty");
                }
                if (!targetTooltip.empty() && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", targetTooltip.c_str());
                }

                if (ImGui::BeginPopup("SelectPrefabProperty"))
                {
                    ImGui::TextUnformatted("Select target property");
                    ImGui::Separator();

                    std::function<void(Entity*)> drawEntityMenu = [&](Entity* ent) {
                        std::string entLabel = ent->getName();
                        if (ent == prefabRoot) entLabel += " (Root)";

                        if (ImGui::BeginMenu(entLabel.c_str()))
                        {
                            if (ImGui::BeginMenu("Transform"))
                            {
                                auto& transform = ent->getTransform();
                                if (ImGui::MenuItem("Position")) {
                                    it->targetComponent = &transform;
                                    it->propertyName = "position";
                                }
                                if (ImGui::MenuItem("Rotation")) {
                                    it->targetComponent = &transform;
                                    it->propertyName = "rotation";
                                }
                                if (ImGui::MenuItem("Scale")) {
                                    it->targetComponent = &transform;
                                    it->propertyName = "scale";
                                }
                                ImGui::EndMenu();
                            }

                            for (Entity* child : ent->getChildren()) {
                                drawEntityMenu(child);
                            }
                            ImGui::EndMenu();
                        }
                    };
                    drawEntityMenu(prefabRoot);
                    ImGui::EndPopup();
                }

                ImGui::TableSetColumnIndex(2);
                if (ImGui::SmallButton("x")) {
                    it = prefabRoot->m_exposedInterfaceCache.erase(it);
                } else {
                    ++it;
                }

                ImGui::PopID();
                ++propIdx;
            }
            ImGui::EndTable();
        }
    }
}

void InspectorPanel::DrawInspector(Entity* selectedEntity)
{
    if (!selectedEntity) return;
    
    if (selectedEntity->isPrefabRoot)
    {
        // РЕЖИМ 2: Дизайнер крутит готовые ползунки на сцене (наш прошлый шаг)
        DrawPrefabInstanceInspector(selectedEntity);
    }
    else
    {
        // РЕЖИМ 3: Обычный инспектор компонентов для уникального объекта сцены
        DrawNormalEntityComponents(selectedEntity);
    }
}
