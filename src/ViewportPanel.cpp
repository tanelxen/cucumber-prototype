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

ViewportPanel::ViewportPanel(const std::string &name) : EditorPanel(name)
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
//    if (m_isHovered || m_pCameraController->isFlyMode())
//    {
//        m_pCameraController->update(dt);
//    }
    
//    if (m_isHovered)
//    {
        m_pCameraController->update(dt);
//    }
}

void ViewportPanel::draw()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
    ImGui::Begin(m_name.c_str());
    
//    {
//        ImVec2 regionMin = ImGui::GetWindowContentRegionMin();
//        ImVec2 regionMax = ImGui::GetWindowContentRegionMax();
//        ImVec2 windowPos = ImGui::GetWindowPos();
//        
//        float minX = windowPos.x + regionMin.x;
//        float minY = windowPos.y + regionMin.y;
//        float maxX = windowPos.x + regionMax.x;
//        float maxY = windowPos.y + regionMax.y;
//        
//        m_viewport->updateBounds(minX, minY, maxX, maxY);
//    }
    
    
    
    float fbo_width = (float)m_viewport->getWidth();
    float fbo_height = (float)m_viewport->getHeight();
    
    ImVec2 current_avail_size = ImGui::GetContentRegionAvail();
    
    
    if (current_avail_size.x != fbo_width || current_avail_size.y != fbo_height)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            m_isResizing = true;
            m_pendingWidth = current_avail_size.x;
            m_pendingHeight = current_avail_size.y;
        }
        else
        {
            m_viewport->resize((int)current_avail_size.x, (int)current_avail_size.y);
            m_isResizing = false;
        }
    }
    
    ImGui::Image((ImTextureID)(intptr_t)m_viewport->getTexture(),
                 current_avail_size,
                 ImVec2(0, 1),
                 ImVec2(1, 0)
                 );
    
    m_isHovered = ImGui::IsItemHovered();
    
    if (m_pCameraController->isFlyMode())
    {
//        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
        ImGui::SetWindowFocus();
    }
    
    ImGui::End();
    ImGui::PopStyleVar();
}
