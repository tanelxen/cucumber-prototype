//
//  MaterialPanel.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 12.08.26.
//

#include "MaterialPanel.h"
#include "Mesh.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "../vendor/tinyfiledialogs.h"
#include "../vendor/stb_image.h"


MaterialPanel::MaterialPanel() : EditorPanel("Material")
{
    
}

void MaterialPanel::setMaterial(Material *material)
{
    m_material = material;
}

static void drawTextCentered(const char* text)
{
    ImVec2 window_size = ImGui::GetWindowSize();
    ImVec2 text_size = ImGui::CalcTextSize(text);
    
    float pos_x = (window_size.x - text_size.x) * 0.5f;
    float pos_y = (window_size.y - text_size.y) * 0.5f;
    
    ImGui::SetCursorPos(ImVec2(pos_x, pos_y));
    
    ImGui::TextUnformatted(text);
}

void MaterialPanel::draw()
{
    ImGui::Begin("Material");
    
    if (m_material == nullptr)
    {
        drawTextCentered("No selected");
        ImGui::End();
        return;
    }
    
    drawContent();
    
    ImGui::End();
}

void MaterialPanel::drawContent()
{
    ImGui::TextUnformatted("Name: ");
    ImGui::SameLine();
    ImGui::TextUnformatted(m_material->m_name.c_str());
    
    ImGui::TextUnformatted("Source: ");
    ImGui::SameLine();
    ImGui::TextUnformatted(m_material->m_source.c_str());
    
    ImGui::Separator();
    
    ImGui::ColorEdit4("Tint Color", &(m_material->tintColor.x), ImGuiColorEditFlags_NoInputs);
    
    ImGui::Image((ImTextureID)(intptr_t)m_material->albedoMap, ImVec2(64, 64));
    
//    {
//        char buffer[256];
//        ImGui::InputText("##AlbedoPath", buffer, 256, ImGuiInputTextFlags_ReadOnly);
//        ImGui::SameLine();
//        if (ImGui::Button("..."))
//        {
//            char const * filter[1] = { "*.png" };
//            const char* path = tinyfd_openFileDialog(NULL, "./assets/meshes/obj/", 1, filter, NULL, 0);
//            
//            if (path != nullptr)
//            {
//                int width, height;
//                int num_channels = 3;
//                
//                unsigned char* image = stbi_load(path, &width, &height, &num_channels, 3);
//                m_material->albedoMap = RenderDevice::makeTexture(width, height, false, true, image);
//                stbi_image_free(image);
//            }
//        }
//    }
    
    ImGui::Separator();
}
