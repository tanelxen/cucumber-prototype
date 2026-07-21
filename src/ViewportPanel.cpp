//
//  ViewportPanel.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#include "ViewportPanel.h"
#include "Viewport.h"

#include "CameraController.h"
#include "Camera.h"

#include <glad/glad.h>
#include <imgui.h>

ViewportPanel::ViewportPanel(const std::string &name) : m_name(name)
{
    m_pCamera = new Camera();
    
    m_pCameraController = new CameraController(m_pCamera);
    m_pCameraController->position = {0, 128, 128};
}

void ViewportPanel::setViewport(Viewport *viewport)
{
    m_viewport = viewport;
    m_viewport->setCamera(m_pCamera);
}

void ViewportPanel::update(float dt)
{
    m_pCameraController->update(dt);
}

void ViewportPanel::draw()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
    ImGui::Begin(m_name.c_str());
    
    {
        ImVec2 regionMin = ImGui::GetWindowContentRegionMin();
        ImVec2 regionMax = ImGui::GetWindowContentRegionMax();
        ImVec2 windowPos = ImGui::GetWindowPos();
        
        float minX = windowPos.x + regionMin.x;
        float minY = windowPos.y + regionMin.y;
        float maxX = windowPos.x + regionMax.x;
        float maxY = windowPos.y + regionMax.y;
        
        m_viewport->updateBounds(minX, minY, maxX, maxY);
    }
    
    ImGui::Image((ImTextureID)(intptr_t)m_viewport->getTexture(),
                 ImVec2(m_viewport->getWidth(), m_viewport->getHeight()),
                 ImVec2(0, 1),
                 ImVec2(1, 0)
                 );
    
    m_isHovered = ImGui::IsItemHovered();
    
    ImGui::End();
    ImGui::PopStyleVar();
}
