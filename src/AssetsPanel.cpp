//
//  AssetsPanel.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#include "AssetsPanel.h"
#include <imgui.h>

void AssetsPanel::draw()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
    ImGui::Begin("Assets");
    
    ImGui::End();
    ImGui::PopStyleVar();
}
