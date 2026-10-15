//
//  HierarchyPanel.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 10.09.26.
//

#include "HierarchyPanel.h"
#include "UIManager.h"

#include "SceneManager.h"
#include "SceneSerializer.h"
#include "World.h"
#include "Entity.h"
#include "ProjectManager.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "../vendor/tinyfiledialogs.h"
#include "../vendor/IconsMaterialDesignIcons.h"

HierarchyPanel::HierarchyPanel() : EditorPanel(ICON_MDI_FILE_TREE " Hierarchy###Hierarchy")
{
    
}

void HierarchyPanel::draw()
{
    auto activeScene = SceneManager::Get().getActiveScene();

    if (m_scene != activeScene)
    {
        m_scene = activeScene;
        m_selectedEntity = nullptr;
        g_UIManager->setEntity(nullptr);
    }
    
    if (!activeScene)
    {
        ImGui::Begin(m_name.c_str()); ImGui::Text("No active scene"); ImGui::End();
        return;
    }
    
    ImGui::Begin(m_name.c_str(), nullptr, ImGuiWindowFlags_NoCollapse);
    
    if (ImGui::Button("+"))
    {
        auto count = (int)activeScene->m_entities.size();
        auto entity = std::make_unique<Entity>("Entity_" + std::to_string(count));
        entity->scene = activeScene;
        
        m_selectedEntity = entity.get();
        g_UIManager->setEntity(m_selectedEntity);
        
        activeScene->m_entities.push_back(std::move(entity));
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Add Prefab"))
    {
        char const * filter[2] = { "*.json", "*.prefab" };
        const auto prefabFolder = ProjectManager::Get().assetsPath() / "prefabs";
        const char* path = tinyfd_openFileDialog("Load Prefab", prefabFolder.string().c_str(), 2, filter, NULL, 0);
        
        if (path != nullptr)
        {
            auto entity = SceneSerializer::InstantiatePrefab(activeScene, path);
            
            if (entity)
            {
                m_selectedEntity = entity;
                g_UIManager->setEntity(m_selectedEntity);
            }
        }
    }
    
    ImGui::Separator();
    
    m_liveEntities.clear();
    for (const auto& entity : activeScene->m_entities)
    {
        if (entity) {
            m_liveEntities.insert(entity.get());
        }
    }

    if (m_selectedEntity && m_liveEntities.contains(m_selectedEntity) && m_selectedEntity->isPendingKill())
    {
        m_selectedEntity = nullptr;
        g_UIManager->setEntity(nullptr);
    }
    else if (m_selectedEntity && !m_liveEntities.contains(m_selectedEntity))
    {
        m_selectedEntity = nullptr;
        g_UIManager->setEntity(nullptr);
    }

    m_rootEntities.clear();
    
    for (auto& entity : activeScene->m_entities)
    {
        if (entity && !entity->isPendingKill() &&
            (!entity->parent || !m_liveEntities.contains(entity->parent))) {
            m_rootEntities.push_back(entity.get());
        }
    }
    
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DrawLinesToNodes;
    if (!m_selectedEntity) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    m_visitedEntities.clear();
    m_pendingReparents.clear();
    m_pendingDestructions.clear();
    if (ImGui::TreeNodeEx(&m_rootEntities, flags, "Scene"))
    {
        if (ImGui::IsItemClicked()) {
            m_selectedEntity = nullptr;
            g_UIManager->setEntity(nullptr);
        }

        for (Entity* entity : m_rootEntities) {
            DrawEntityNode(entity);
        }
        ImGui::TreePop();
    }

    for (Entity* entity : m_pendingDestructions) {
        if (m_liveEntities.contains(entity)) {
            entity->destroy();
        }
    }

    for (const auto& [child, parent] : m_pendingReparents)
    {
        if (m_liveEntities.contains(child) && m_liveEntities.contains(parent) &&
            !child->isPendingKill() && !parent->isPendingKill()) {
            child->setParent(parent, true);
        }
    }

    ImVec2 minPos = ImGui::GetWindowPos();
    ImVec2 maxPos = ImVec2(minPos.x + ImGui::GetWindowWidth(), minPos.y + ImGui::GetWindowHeight());
    if (ImGui::BeginDragDropTargetCustom(ImRect(minPos, maxPos), ImGui::GetID("HierarchyEmptySpace"))) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
            if (payload->Data && payload->DataSize == sizeof(Entity*)) {
                Entity* draggedEntity = *static_cast<Entity* const*>(payload->Data);
                if (m_liveEntities.contains(draggedEntity) && !draggedEntity->isPendingKill()) {
                    draggedEntity->setParent(nullptr, true);
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
    
    ImGui::End();
}

void HierarchyPanel::DrawEntityNode(Entity* entity)
{
    if (!entity || !m_liveEntities.contains(entity) || entity->isPendingKill() ||
        !m_visitedEntities.insert(entity).second) {
        return;
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DrawLinesToNodes;
    
    if (m_selectedEntity == entity) flags |= ImGuiTreeNodeFlags_Selected;
    if (entity->children.empty())  flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;
    
    // Отрисовываем узел дерева
    bool isOpened = ImGui::TreeNodeEx((void*)(uintptr_t)entity, flags, entity->isPrefabRoot ? "[P] %s" : ICON_MDI_CUBE " %s", entity->m_name.c_str());
    
    // Умное выделение объекта с учетом, что это может быть просто drag-n-drop
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::IsItemHovered(ImGuiHoveredFlags_None))
    {
        m_selectedEntity = entity;
        g_UIManager->setEntity(m_selectedEntity);
    }
    
    // --- 1. DRAG-N-DROP: ИСТОЧНИК (Тащим этот объект) ---
    if (ImGui::BeginDragDropSource())
    {
        ImGui::SetDragDropPayload("HIERARCHY_ENTITY", &entity, sizeof(Entity*));
        ImGui::Text("Move: %s", entity->m_name.c_str());
        ImGui::EndDragDropSource();
    }
    
    // --- 2. DRAG-N-DROP: ПРИЕМНИК (Бросаем ДРУГОЙ объект на этот) ---
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY"))
        {
            Entity* draggedEntity = payload->Data && payload->DataSize == sizeof(Entity*)
                ? *static_cast<Entity* const*>(payload->Data)
                : nullptr;
            
            // Защита: объект не может стать родителем самого себя
            // И не может стать ребенком своего собственного ребенка (цикл)
            if (draggedEntity && m_liveEntities.contains(draggedEntity) &&
                !draggedEntity->isPendingKill() && draggedEntity != entity &&
                !IsChildOf(entity, draggedEntity))
            {
                m_pendingReparents.emplace_back(draggedEntity, entity);
            }
        }
        
        ImGui::EndDragDropTarget();
    }
    
    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Удалить объект"))
        {
            m_pendingDestructions.push_back(entity);
            if (m_selectedEntity == entity) {
                m_selectedEntity = nullptr;
                g_UIManager->setEntity(nullptr);
            }
        }
        
        // Если этот объект на сцене пришел из префаба — даем возможность его открыть!
        if (!entity->prefabAssetPath.empty() && ImGui::MenuItem("Редактировать префаб"))
        {
            // Открываем префаб, сцена уровня автоматически скрывается в бэкграунд!
            SceneManager::Get().enterPrefabEditMode(entity->prefabAssetPath);
        }
        
        if (ImGui::MenuItem("Сохранить как префаб"))
        {
            char const * filter[2] = { "*.prefab", "*.json" };
            const auto prefabFolder = ProjectManager::Get().assetsPath() / "prefabs";
            const char* path = tinyfd_saveFileDialog("Save Prefab", prefabFolder.string().c_str(), 2, filter, NULL);
            
            if (path != nullptr)
            {
                auto activeScene = SceneManager::Get().getActiveScene();
                SceneSerializer::CreatePrefabFromEntity(activeScene, entity, path);
            }
        }
        
        ImGui::EndPopup();
    }
    
    // Рекурсивный вызов для отрисовки детей, если папка раскрыта
    if (isOpened) {
        for (Entity* child : entity->children) {
            DrawEntityNode(child);
        }
        ImGui::TreePop();
    }
}

bool HierarchyPanel::IsChildOf(Entity* potentialChild, Entity* potentialParent) const
{
    if (!potentialChild || !potentialParent) {
        return false;
    }

    std::unordered_set<Entity*> visited;
    Entity* current = potentialChild->parent;
    while (current && m_liveEntities.contains(current) && visited.insert(current).second) {
        if (current == potentialParent) return true;
        current = current->parent;
    }
    return false;
}
