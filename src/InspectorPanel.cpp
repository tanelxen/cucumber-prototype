//
//  ViewportPanel.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#include "InspectorPanel.h"

#include "Mesh.h"
#include "ModelLoader.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <stdio.h>
#include <iostream>

#include "../vendor/tinyfiledialogs.h"

void InspectorPanel::setEntity(Mesh *entity)
{
    m_entity = entity;
}

bool DrawVector3Control(const char* label, float* values, float resetValue, float speed);
void DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue, float columnWidth);

void DrawTransformComponent(Mesh& entity)
{
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
    {
//        DrawVector3Control("Position", &entity.position.x, 0.0f, 0.05f);
//        DrawVector3Control("Rotation", &entity.rotation.x, 0.0f, 0.1f);
//        DrawVector3Control("Scale", &entity.scale.x, 1.0f, 0.01f);
        
        DrawVec3Control("Position", entity.position, 0, 100);
        DrawVec3Control("Rotation", entity.rotation, 0, 100);
        DrawVec3Control("Scale", entity.scale, 64, 100);
    }
}



void InspectorPanel::draw()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
    ImGui::Begin("Inspector");
    
    ImGui::SeparatorText("Entity");
    
    char buffer[256] = "";
    snprintf(buffer, sizeof(buffer), "%s", m_entity->m_name.c_str());
    
    if (ImGui::InputText("##Name", buffer, sizeof(buffer)))
    {
        auto& name = m_entity->m_name;
        name = buffer;
    }
    
    
    ImGui::SeparatorText("Components");
    
    DrawTransformComponent(*m_entity);
    
    if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::ColorEdit3("Color", &m_entity->color.x);
    }
    
    if (ImGui::Button("Add Mesh"))
    {
        char const * filter[1] = { "*.obj" };
        const char* path = tinyfd_openFileDialog(NULL, "./assets/meshes/", 1, filter, NULL, 0);
        
        if (path != nullptr)
        {
            ModelLoader::LoadOBJ(path, *m_entity);
        }
    }
    
    
    if (ImGui::Button("Add Component"))
    {
        ImGui::OpenPopup("add_component_popup");
    }
    
    if (ImGui::BeginPopup("add_component_popup"))
    {
        ImGui::TextDisabled("Componentes");
        ImGui::Separator();
        
        if (ImGui::Selectable("Transform"))
            std::cout << "Add Transform" << std::endl;

        if (ImGui::Selectable("Mesh Renderer"))
            std::cout << "Add Mesh Renderer" << std::endl;
        
        if (ImGui::Selectable("Light"))
            std::cout << "Add Light" << std::endl;
        
        if (ImGui::Selectable("Script"))
            std::cout << "Add Script" << std::endl;
        
        ImGui::EndPopup();
    }
    
    ImGui::End();
    ImGui::PopStyleVar();
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



void DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f)
{
    ImGuiIO& io = ImGui::GetIO();
    auto boldFont = io.Fonts->Fonts[0];
    
    ImGui::PushID(label.c_str());
    
    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, columnWidth);
    ImGui::Text("%s", label.c_str());
    ImGui::NextColumn();
    
    ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });
    
    float lineHeight = 16.0f + ImGui::GetStyle().FramePadding.y * 2.0f;
    ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
    ImGui::PushFont(boldFont);
    if (ImGui::Button("X", buttonSize))
        values.x = resetValue;
    ImGui::PopFont();
    ImGui::PopStyleColor(3);
    
    ImGui::SameLine();
    ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
    ImGui::PopItemWidth();
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
    ImGui::PushFont(boldFont);
    if (ImGui::Button("Y", buttonSize))
        values.y = resetValue;
    ImGui::PopFont();
    ImGui::PopStyleColor(3);
    
    ImGui::SameLine();
    ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
    ImGui::PopItemWidth();
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
    ImGui::PushFont(boldFont);
    if (ImGui::Button("Z", buttonSize))
        values.z = resetValue;
    ImGui::PopFont();
    ImGui::PopStyleColor(3);
    
    ImGui::SameLine();
    ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
    ImGui::PopItemWidth();
    
    ImGui::PopStyleVar();
    
    ImGui::Columns(1);
    
    ImGui::PopID();
}
