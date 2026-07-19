//
//  ViewportPanel.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#include "InspectorPanel.h"
#include <imgui.h>

//void InspectorPanel::setEntity(Entity *entity)
//{
//    m_entity = entity;
//}

void InspectorPanel::draw()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
    ImGui::Begin("Inspector");
    
    ImGui::End();
    ImGui::PopStyleVar();
}
