//
//  ViewportPanel.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#include "InspectorPanel.h"
#include "UIManager.h"

#include "Mesh.h"
#include "ModelLoader.h"

#include "Entity.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <stdio.h>
#include <iostream>
#include <filesystem>

#include <unordered_map>
#include <typeindex>
#include <functional>

#include "../vendor/tinyfiledialogs.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../vendor/stb_image.h"

#include "RenderDevice.h"

class ComponentInspector {
public:
    // Сигнатура функции отрисовки: принимает чистый указатель на компонент
    using RenderFunc = std::function<void(Component*, InspectorPanel* p)>;
    
    static ComponentInspector& Get() {
        static ComponentInspector instance;
        return instance;
    }
    
    // Регистрация нового UI для компонента
    template<typename T>
    void Register(const std::string& displayName, RenderFunc func) {
        std::type_index id = std::type_index(typeid(T));
        registry[id] = { displayName, func };
    }
    
    // Отрисовка компонента по его динамическому типу
    void Render(Component* component, InspectorPanel* panel) {
        if (!component) return;
        
        std::type_index id = component->GetTypeIndex();
        auto it = registry.find(id);
        
        if (it != registry.end()) {
            const auto& info = it->second;
            
            // Отрисовываем красивый заголовок в ImGui
            if (ImGui::CollapsingHeader(info.displayName.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
//                ImGui::Checkbox("Enabled", &component->enabled);
                // Вызываем кастомную функцию отрисовки
                info.renderFunc(component, panel);
            }
        } else {
            // Фолбэк, если UI для компонента забыли зарегистрировать
            ImGui::CollapsingHeader(typeid(*component).name());
        }
    }
    
    // Дополнительный метод для получения красивого имени (для меню добавления)
    std::string GetName(std::type_index id) {
        auto it = registry.find(id);
        return (it != registry.end()) ? it->second.displayName : id.name();
    }
    
private:
    struct InspectorInfo {
        std::string displayName;
        RenderFunc renderFunc;
    };
    
    std::unordered_map<std::type_index, InspectorInfo> registry;
};

static void RegisterUI();

void InspectorPanel::setEntity(Entity *entity)
{
    m_entity = entity;
    RegisterUI();
}

//bool DrawVector3Control(const char* label, float* values, float resetValue, float speed);
bool DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue, float columnWidth);

static void RegisterUI()
{
    auto& inspector = ComponentInspector::Get();
    
    inspector.Register<TransformComponent>("Transform", [](Component* c, InspectorPanel* p) {
        auto* comp = static_cast<TransformComponent*>(c);
        DrawVec3Control("Position", comp->position, 0, 100);
        DrawVec3Control("Rotation", comp->rotation, 0, 100);
        DrawVec3Control("Scale", comp->scale, 1, 100);
    });
    
    inspector.Register<MeshComponent>("Mesh", [](Component* c, InspectorPanel* p) {
        auto* comp = static_cast<MeshComponent*>(c);
        
//        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
        
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "%s", comp->path.c_str());
        
        ImGui::InputText("##MeshPath", buffer, 256, ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        if (ImGui::Button("..."))
        {
            char const * filter[2] = { "*.gltf", "*.glb" };
            const char* path = tinyfd_openFileDialog(NULL, "./assets/meshes/gltf/", 2, filter, NULL, 0);
            
            if (path != nullptr)
            {
                ModelLoader::LoadGLTF(path, comp->mesh, comp->animator);
                
                std::filesystem::path p(path);
                comp->path = p.filename();
            }
        }
        
        if (!comp->mesh.m_surfaces.empty())
        {
            if (ImGui::TreeNode("Surfaces"))
            {
                for (int i = 0; i < comp->mesh.m_surfaces.size(); i++)
                {
                    if (ImGui::TreeNode((void*)(intptr_t)i, "Surface %i", i))
                    {
                        auto& surface = comp->mesh.m_surfaces[i];
                        ImGui::Text("%s: %i tris, %i material", surface.m_name.c_str(), surface.count / 3, surface.materialSlot);
                        
                        auto& material = comp->mesh.m_materials[surface.materialSlot];
                        
                        if (ImGui::Button("Edit material"))
                        {
                            p->m_manager->setMaterial(&material);
                        }
                        
                        ImGui::TreePop();
                    }
                }
                
                ImGui::TreePop();
            }
        }
        
        if (!comp->animator.m_clips.empty())
        {
            auto& animator = comp->animator;
            auto& clips = animator.m_clips;
            
            const char* title = animator.m_currentClipIndex == -1 ? "<no selected>" : clips[animator.m_currentClipIndex].name.c_str();
            
            if (ImGui::BeginCombo("##sequence combo", title, ImGuiComboFlags_None))
            {
                for (int i = 0; i < clips.size(); ++i)
                {
                    bool is_selected = (animator.m_currentClipIndex == i);
                    
                    if (ImGui::Selectable(clips[i].name.c_str(), is_selected))
                    {
                        animator.play(i);
                    }
                    
                    if (is_selected) ImGui::SetItemDefaultFocus();
                }
                
                ImGui::EndCombo();
            }
        }
    });
}

void InspectorPanel::draw()
{
//    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{16, 16});
    ImGui::Begin("Inspector");
    
    if (!m_entity)
    {
        ImGui::End();
//        ImGui::PopStyleVar();
        return;
    }
    
    ImGui::Checkbox("##active", &m_entity->m_isActive);
    
    ImGui::SameLine();
    
    char buffer[256] = "";
    snprintf(buffer, sizeof(buffer), "%s", m_entity->m_name.c_str());
    
    if (ImGui::InputText("##Name", buffer, sizeof(buffer)))
    {
        auto& name = m_entity->m_name;
        name = buffer;
    }
    
    
    ImGui::Separator();
    
    for (auto& component : m_entity->getComponents())
    {
        ComponentInspector::Get().Render(component.get(), this);
        ImGui::Spacing();
    }
    
    ImGui::Separator();
    ImGui::Spacing();
    
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentPopup");
    }
    
    if (ImGui::BeginPopup("AddComponentPopup")) {
        if (ImGui::MenuItem("Transform")) { m_entity->addComponent<TransformComponent>(); }
        if (ImGui::MenuItem("Mesh"))     { m_entity->addComponent<MeshComponent>(); }
        ImGui::EndPopup();
    }
    
//    if (ImGui::Button("Add Component"))
//    {
//        ImGui::OpenPopup("add_component_popup");
//    }
//    
//    if (ImGui::BeginPopup("add_component_popup"))
//    {
//        ImGui::TextDisabled("Componentes");
//        ImGui::Separator();
//        
//        if (ImGui::Selectable("Transform")) {
//            std::cout << "Add Transform" << std::endl;
//            m_entity->addComponent<TransformComponent>();
//        }
//
//        if (ImGui::Selectable("Mesh")) {
//            std::cout << "Add Mesh Renderer" << std::endl;
//            m_entity->addComponent<MeshComponent>();
//        }
//        
//        if (ImGui::Selectable("Light")) {
//            std::cout << "Add Light" << std::endl;
//        }
//        
//        if (ImGui::Selectable("Script")) {
//            std::cout << "Add Script" << std::endl;
//        }
//        
//        ImGui::EndPopup();
//    }
    
    ImGui::End();
//    ImGui::PopStyleVar();
}

InspectorPanel::InspectorPanel() : EditorPanel("Inspector")
{
    
}


// Вспомогательная функция для отрисовки одной оси с цветной кнопкой-сбросом
bool DrawAxisControl(const char* label, float& value, const ImVec4& color, float resetValue, float speed, float width) {
    bool modified = false;
    
    // Стилизуем кнопку (красный, зеленый или синий цвет)
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x + 0.1f, color.y + 0.1f, color.z + 0.1f, color.w));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
    
    // Кнопка сброса оси (например, "X")
    ImGui::PushID(label);
    if (ImGui::Button(label, ImVec2(20, 0))) {
        value = resetValue;
        modified = true;
    }
    ImGui::PopID();
    ImGui::PopStyleColor(3);
    
    ImGui::SameLine(0, 2); // Минимальный отступ между кнопкой и полем ввода
    ImGui::SetNextItemWidth(width - 22); // Вычитаем ширину кнопки и отступа
    
    // Поле ввода значения
    std::string dragLabel = "##" + std::string(label);
    if (ImGui::DragFloat(dragLabel.c_str(), &value, speed, 0.0f, 0.0f, "%.2f")) {
        modified = true;
    }
    
    return modified;
}

// Основная функция для отрисовки трех осей (вектора)
bool DrawVector3Control(const char* label, float* values, float resetValue = 0.0f, float speed = 0.1f)
{
    bool modified = false;
    
    ImGui::BeginGroup();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.3f); // Выравниваем поля ввода по сетке
    
    // Вычисляем доступную ширину для трех полей с учетом отступов стиля
    float totalWidth = ImGui::GetContentRegionAvail().x;
    float itemWidth = (totalWidth - (ImGui::GetStyle().ItemSpacing.x * 2.0f)) / 3.0f;
    
    // Цвета в стиле Unreal Engine (Красный, Зеленый, Синий)
    ImVec4 colorX = ImVec4(0.7f, 0.1f, 0.15f, 1.0f);
    ImVec4 colorY = ImVec4(0.2f, 0.7f, 0.2f, 1.0f);
    ImVec4 colorZ = ImVec4(0.1f, 0.4f, 0.7f, 1.0f);
    
    // Ось X
    if (DrawAxisControl("X", values[0], colorX, resetValue, speed, itemWidth)) modified = true;
    
    // Ось Y
    ImGui::SameLine();
    if (DrawAxisControl("Y", values[1], colorY, resetValue, speed, itemWidth)) modified = true;
    
    // Ось Z
    ImGui::SameLine();
    if (DrawAxisControl("Z", values[2], colorZ, resetValue, speed, itemWidth)) modified = true;
    
    ImGui::EndGroup();
    return modified;
}


bool DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue, float columnWidth)
{
    ImGuiIO& io = ImGui::GetIO();
    auto boldFont = io.Fonts->Fonts[0];
    
    ImGui::PushID(label.c_str());
    
    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, columnWidth);
    ImGui::Text(label.c_str());
    ImGui::NextColumn();
    
    ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });
    
    float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
    ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };
    
    bool isChangedX = false;
    bool isChangedY = false;
    bool isChangedZ = false;
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.20f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
    
    ImGui::PushFont(boldFont);
    if (ImGui::Button("X", buttonSize)) {
        values.x = resetValue;
        isChangedX = true;
    }
    ImGui::PopFont();
    
    ImGui::PopStyleColor(3);
    
    ImGui::SameLine();
    if (ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f")) {
        isChangedX = true;
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
    
    ImGui::PushFont(boldFont);
    if (ImGui::Button("Y", buttonSize)) {
        values.y = resetValue;
        isChangedY = true;
    }
    ImGui::PopFont();
    
    ImGui::PopStyleColor(3);
    
    ImGui::SameLine();
    if (ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f")) {
        isChangedY = true;
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
    
    ImGui::PushFont(boldFont);
    if (ImGui::Button("Z", buttonSize)) {
        values.z = resetValue;
        isChangedZ = true;
    }
    ImGui::PopFont();
    
    ImGui::PopStyleColor(3);
    
    ImGui::SameLine();
    if (ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f")) {
        isChangedZ = true;
    }
    
    ImGui::PopItemWidth();
    
    ImGui::PopStyleVar();
    
    ImGui::Columns(1);
    
    ImGui::PopID();
    
    return isChangedX || isChangedY || isChangedZ;
}
