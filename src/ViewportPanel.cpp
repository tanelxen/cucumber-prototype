//
//  ViewportPanel.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#include "ViewportPanel.h"
#include "Viewport.h"

#include <glad/glad.h>
#include <imgui.h>

ViewportPanel::ViewportPanel(const std::string &name) : m_name(name) { }

void ViewportPanel::setViewport(Viewport *viewport)
{
    m_viewport = viewport;
}

void ViewportPanel::draw()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
    ImGui::Begin(m_name.c_str());
    
    ImVec2 size = ImGui::GetContentRegionAvail();
    
    if (size.x != m_viewport->getWidth() || size.y != m_viewport->getHeight())
    {
        m_viewport->resize(size.x, size.y);
    }
    
    ImTextureID texture = (ImTextureID)(intptr_t)m_viewport->getTexture();
    ImGui::Image(texture, size, ImVec2(0, 1), ImVec2(1, 0));
    
    ImGui::End();
    ImGui::PopStyleVar();
}
