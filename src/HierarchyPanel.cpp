//
//  HierarchyPanel.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 10.09.26.
//

#include "HierarchyPanel.h"
#include "World.h"
#include "Entity.h"

#include <imgui.h>
#include <imgui_internal.h>

std::vector<Entity*> rootEntities {};
Entity* selectedEntity;

HierarchyPanel::HierarchyPanel() : EditorPanel("Hierarchy")
{
    
}

void HierarchyPanel::draw()
{
    ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoCollapse);
    
    if (ImGui::Button("+"))
    {
        auto count = (int)g_currentScene->m_entities.size();
        auto entity = std::make_unique<Entity>("Entity_" + std::to_string(count));
        
        g_currentScene->m_entities.push_back(std::move(entity));
        g_currentScene->selectEntityAtIndex(count);
//        selectedIndex = count;
    }
    
    ImGui::Separator();
    
    rootEntities.clear();
    
    for (auto& entity : g_currentScene->m_entities)
    {
        if (entity->parent == nullptr) {
            rootEntities.push_back(entity.get());
        }
    }
    
    Draw(rootEntities, selectedEntity);
    
    ImGui::End();
}

void HierarchyPanel::Draw(const std::vector<Entity*>& rootEntities, Entity*& selectedEntity)
{
    ImGui::Begin("Hierarchy");
    
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DrawLinesToNodes;
    
//    if (selectedIndex == -1) {
//        flags |= ImGuiTreeNodeFlags_Selected;
//    }
    
    if (ImGui::TreeNodeEx(&g_currentScene, flags, "Scene"))
    {
        
        // Отрисовываем только корневые сущности (у которых нет родителя)
        for (Entity* entity : rootEntities) {
            if (entity->parent == nullptr) {
                DrawEntityNode(entity, selectedEntity);
            }
        }
        
        ImGui::TreePop();
    }
    
    // Drop-зона на пустое место панели (чтобы сделать объект снова корневым)
    ImVec2 minPos = ImGui::GetWindowPos();
    ImVec2 maxPos = ImVec2(minPos.x + ImGui::GetWindowWidth(), minPos.y + ImGui::GetWindowHeight());
    if (ImGui::BeginDragDropTargetCustom(ImRect(minPos, maxPos), ImGui::GetID("HierarchyEmptySpace"))) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
            Entity* draggedEntity = *static_cast<Entity**>(payload->Data);
            if (draggedEntity) {
                draggedEntity->setParent(nullptr, true); // Отцепляем от родителя
            }
        }
        ImGui::EndDragDropTarget();
    }
    
    ImGui::End();
}

void HierarchyPanel::DrawEntityNode(Entity* entity, Entity*& selectedEntity)
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DrawLinesToNodes;
    
    if (selectedEntity == entity) flags |= ImGuiTreeNodeFlags_Selected;
    if (entity->children.empty())  flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;
    
    // Отрисовываем узел дерева
    bool isOpened = ImGui::TreeNodeEx((void*)(uintptr_t)entity, flags, "%s", entity->m_name.c_str());
    
    // Клик — выделение объекта
    if (ImGui::IsItemClicked()) {
        selectedEntity = entity;
    }
    
    // --- 1. DRAG-N-DROP: ИСТОЧНИК (Тащим этот объект) ---
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("HIERARCHY_ENTITY", &entity, sizeof(Entity*));
        ImGui::Text("Move: %s", entity->m_name.c_str());
        ImGui::EndDragDropSource();
    }
    
    // --- 2. DRAG-N-DROP: ПРИЕМНИК (Бросаем ДРУГОЙ объект на этот) ---
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
            Entity* draggedEntity = *static_cast<Entity**>(payload->Data);
            
            // Защита: объект не может стать родителем самого себя
            // И не может стать ребенком своего собственного ребенка (цикл)
            if (draggedEntity && draggedEntity != entity && !IsChildOf(entity, draggedEntity)) {
                draggedEntity->setParent(entity, true);
            }
        }
        ImGui::EndDragDropTarget();
    }
    
    // Рекурсивный вызов для отрисовки детей, если папка раскрыта
    if (isOpened) {
        for (Entity* child : entity->children) {
            DrawEntityNode(child, selectedEntity);
        }
        ImGui::TreePop();
    }
}

bool HierarchyPanel::IsChildOf(Entity* potentialChild, Entity* potentialParent)
{
    Entity* current = potentialChild->parent;
    while (current != nullptr) {
        if (current == potentialParent) return true;
        current = current->parent;
    }
    return false;
}

