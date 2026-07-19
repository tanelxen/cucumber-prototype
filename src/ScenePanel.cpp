//
//  ViewportPanel.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#include "ScenePanel.h"
#include "World.h"

#include <glad/glad.h>
#include <imgui.h>

void ScenePanel::setScene(WorldScene *scene)
{
    m_scene = scene;
}

void ScenePanel::draw()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
    ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoCollapse);
    
    ImGui::End();
    ImGui::PopStyleVar();
}
