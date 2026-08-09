//
//  AssetsPanel.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#include "AssetsPanel.h"
#include "RenderDevice.h"

#include <imgui.h>

#include "../vendor/stb_image.h"

#include <filesystem>
#include <set>

//namespace fs = std::filesystem;
//using Path = std::filesystem::path;
//using std::filesystem::exists;

const std::filesystem::path rootDir = ".";
std::filesystem::path currentDir = "assets";
std::set<std::filesystem::path> selected;

static inline ImTextureID IconFolder;
static inline ImTextureID IconFile;

void ChangeDir(std::filesystem::path path)
{
    currentDir = path;
    selected.clear();
}

namespace GUI
{
    bool MenuBarButton(std::string_view title)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
        bool clicked = ImGui::Button(title.data());
        ImGui::PopStyleColor();
        return clicked;
    }
    
    bool Thumbnail(const char* name, ImTextureID icon, bool selected)
    {
        bool clicked = false;
        ImVec2 itemSize = {80, 80};
        float iconSize = 48;
        float iconPadding = (itemSize.x - iconSize) * 0.5;
        float textSize = ImGui::CalcTextSize(name).x;
        float textPadding = (itemSize.x - textSize) * 0.5;
        
        ImGui::BeginGroup();
        if (ImGui::Selectable((std::string("##") + name).c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick, itemSize)) {
            clicked = true;
        }
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - itemSize.y);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + iconPadding);
        
        if (icon != 0)
            ImGui::Image(icon, {iconSize, iconSize});
        
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + textPadding);
        ImGui::TextUnformatted(name);
        ImGui::EndGroup();
        
        return clicked;
    }
}

void DrawFiles()
{
    // TODO: Cache directory file list
//    for (auto file : std::filesystem::directory_iterator(currentDir))
//    {
//        auto path = file.path();
//        bool dir = file.is_directory();
//        
//        if (GUI::Thumbnail(path.filename().string().c_str(), dir ? IconFolder : IconFile, selected.contains(path)))
//        {
//            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
//            {
//                if (dir) {
//                    ChangeDir(path);
//                }
//            }
//            else
//            {
//                if (ImGui::GetIO().KeyCtrl) // Multi-select / deselect
//                {
//                    if (selected.contains(path))
//                        selected.erase(path);
//                    else
//                        selected.insert(path);
//                }
//                else // Single selection
//                {
//                    selected.clear();
//                    selected.insert(path);
//                }
//            }
//        }
//        ImGui::SameLine();
//    }
    
    int _cellSize = 80;
    
    if (ImGui::BeginChild("##gallery", ImVec2(0, 0), false))
    {
        float panelWidth = ImGui::GetContentRegionAvail().x;
        float cellWidth = _cellSize + 8.f;
        int columns = std::max(1, (int)(panelWidth / cellWidth));
        
        int i = 0;
        
        for (auto file : std::filesystem::directory_iterator(currentDir))
        {
            auto path = file.path();
            bool dir = file.is_directory();
            
            int col = i % columns;
            if (col != 0) ImGui::SameLine();
            
            ImGui::BeginGroup();
            
            auto icon = dir ? IconFolder : IconFile;
            
            ImGui::PushID(i);
            ImGui::ImageButton("##img", (ImTextureID)(intptr_t)icon, ImVec2(_cellSize, _cellSize));
            
            if (ImGui::IsItemHovered())
            {
                if (ImGui::IsMouseDoubleClicked(0))
                {
                    //                    if (node.isDirectory) {
                    //                        nodeToChange = &*it;
                    //                        changeNode = true;
                    //                    }
                    //                    else {
                    //                        openFile(node.fullPath, node.fileType);
                    //                    }
                }
            }
            
            ImGui::PopID();
            
            // Nombre truncado debajo
            std::string name = path.filename().string();
            
            if (name.size() > 20) name = name.substr(0, 16) + "...";
            ImGui::TextUnformatted(name.c_str());
            
            //            // Tooltip con nombre completo
            //            if (ImGui::IsItemHovered())
            //                ImGui::SetTooltip("%s", node.name.c_str());
            
            ImGui::EndGroup();
            
            i++;
        }
        
        //        if (changeNode) {
        //            _selectedNode = nodeToChange;
        //        }
    }
    
    ImGui::EndChild();
}

AssetsPanel::AssetsPanel()
{
    {
        int width, height;
        int num_channels = 3;
        
        unsigned char* image = stbi_load("assets/icons/folder.png", &width, &height, &num_channels, 3);
        IconFolder = RenderDevice::makeTexture(width, height, false, true, image);
        stbi_image_free(image);
    }
    
    {
        int width, height;
        int num_channels = 3;
        
        unsigned char* image = stbi_load("assets/icons/file.png", &width, &height, &num_channels, 3);
        IconFile = RenderDevice::makeTexture(width, height, false, true, image);
        stbi_image_free(image);
    }
}

void AssetsPanel::draw()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
    ImGui::Begin("Assets", nullptr, ImGuiWindowFlags_MenuBar);
    
    if (ImGui::BeginMenuBar())
    {
        ImGui::TextUnformatted("Files");
        
        std::filesystem::path path = rootDir;
        
        for (auto dir : std::filesystem::relative(currentDir, rootDir))
        {
            path = path / dir;
            
            ImGui::TextUnformatted(">");
            
            if (GUI::MenuBarButton(dir.string()))
            {
                ChangeDir(path);
            }
        }
        
        ImGui::EndMenuBar();
    }
    
    DrawFiles();
    
    ImGui::End();
    ImGui::PopStyleVar();
}
