//
//  ViewportPanel.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#include "InspectorPanel.h"

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

void RegisterUI();

void InspectorPanel::setEntity(Entity *entity)
{
    m_entity = entity;
    RegisterUI();
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
        
//        DrawVec3Control("Position", entity.position, 0, 100);
//        DrawVec3Control("Rotation", entity.rotation, 0, 100);
//        DrawVec3Control("Scale", entity.scale, 64, 100);
    }
}


std::unordered_map<std::type_index, std::function<void(Component*)>> uiRegistry;

void RegisterUI()
{
    uiRegistry[typeid(TransformComponent)] = [](Component* c) {
        auto* comp = static_cast<TransformComponent*>(c);
        
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            DrawVec3Control("Position", comp->position, 0, 100);
            DrawVec3Control("Rotation", comp->rotation, 0, 100);
            DrawVec3Control("Scale", comp->scale, 1, 100);
        }
    };
    
    uiRegistry[typeid(MeshComponent)] = [](Component* c) {
        auto* comp = static_cast<MeshComponent*>(c);
        
        if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
            
//            ImGui::TextUnformatted(comp->path.c_str());
//            ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.3f);
            
//            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5);
            
            char buffer[256];
            
            ImGui::InputText("##MeshPath", buffer, 256, ImGuiInputTextFlags_ReadOnly);
            ImGui::SameLine();
            if (ImGui::Button("..."))
            
//            if (ImGui::Button("Add Mesh"))
            {
                char const * filter[1] = { "*.obj" };
                const char* path = tinyfd_openFileDialog(NULL, "./assets/meshes/obj/", 1, filter, NULL, 0);
                
                if (path != nullptr)
                {
                    ModelLoader::LoadOBJ(path, comp->mesh);
                    
                    std::filesystem::path p(path);
                    comp->path = p.filename();
                }
            }
            
            if (ImGui::TreeNode("Surfaces"))
            {
                for (int i = 0; i < comp->mesh.m_surfaces.size(); i++)
                {
                    if (ImGui::TreeNode((void*)(intptr_t)i, "Surface %i", i))
                    {
                        auto& surface = comp->mesh.m_surfaces[i];
                        ImGui::Text("%s: %i tris, %i material", surface.m_name.c_str(), surface.IndexCount / 3, surface.MaterialIndex);
                        
                        auto& material = comp->mesh.m_materials[surface.MaterialIndex];
                        
                        ImGui::ColorEdit4("Tint Color", &(material.tintColor.x), ImGuiColorEditFlags_NoInputs);
                        
                        if (ImGui::Button("Add Texture"))
                        {
                            char const * filter[1] = { "*.png" };
                            const char* path = tinyfd_openFileDialog(NULL, "./assets/meshes/obj/", 1, filter, NULL, 0);
                            
                            if (path != nullptr)
                            {
                                int width, height;
                                int num_channels = 3;
                                
                                unsigned char* image = stbi_load(path, &width, &height, &num_channels, 3);
                                material.albedoMap = RenderDevice::makeTexture(width, height, false, true, image);
                                stbi_image_free(image);
                            }
                        }
                        
                        ImGui::TreePop();
                    }
                }
                
                ImGui::TreePop();
            }
        }
    };
}

void drawComponentUi(std::type_index typeIdx, Component* comp)
{
    auto it = uiRegistry.find(typeIdx);
    
    if (it != uiRegistry.end())
    {
//        const auto& uiInfo = it->second;
        
//        // Используем ImGui::TreeNodeEx для создания выпадающего меню компонента
//        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth;
//        
//        if (ImGui::TreeNodeEx(uiInfo.displayName.c_str(), flags)) {
//            
//            // Вызываем кастомный UI для этого компонента
//            uiInfo.renderUiFunc(comp);
//            
//            ImGui::TreePop();
//        }
//        
//        ImGui::Separator();
        
        const auto& uiInfo = it->second;
        uiInfo(comp);
    }
}

void InspectorPanel::draw()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
    ImGui::Begin("Inspector");
    
    if (!m_entity)
    {
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }
    
    ImGui::SeparatorText("Entity");
    
    static bool active = true;
    ImGui::Checkbox("##active", &active);
    
    ImGui::SameLine();
    
    char buffer[256] = "";
    snprintf(buffer, sizeof(buffer), "%s", m_entity->m_name.c_str());
    
    if (ImGui::InputText("##Name", buffer, sizeof(buffer)))
    {
        auto& name = m_entity->m_name;
        name = buffer;
    }
    
    
    ImGui::SeparatorText("Components");
    
    for (auto& [typeIdx, comp] : m_entity->m_components)
    {
        drawComponentUi(typeIdx, comp.get());
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
