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
#include "ViewportPanel.h"

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
        ImGui::OpenPopup("CreateEntityPopup");
    }

    if (ImGui::BeginPopup("CreateEntityPopup"))
    {
        const glm::ivec2 viewportSize = g_UIManager->m_pViewportPanel->viewport().size();
        const glm::vec2 viewportCenter = glm::vec2(viewportSize) * 0.5f;
        const glm::vec3 creationPosition =
            g_UIManager->m_pViewportPanel->getCreationPositionAt(viewportCenter);
        g_UIManager->drawEntityCreationMenu(*activeScene, creationPosition);
        ImGui::EndPopup();
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

    const std::string documentName = activeScene->m_filePath.empty()
        ? "Untitled"
        : std::filesystem::path(activeScene->m_filePath).stem().string();
    const bool isPrefab = activeScene->isPrefabContext();
    ImGui::PushStyleColor(ImGuiCol_Text, isPrefab
        ? ImVec4(0.60f, 1.00f, 0.79f, 1.0f)
        : ImVec4(0.60f, 0.78f, 1.00f, 1.0f));
    ImGui::Text("%s %s: %s",
                isPrefab ? ICON_MDI_CUBE_OUTLINE : ICON_MDI_FILE_DOCUMENT_OUTLINE,
                isPrefab ? "Prefab" : "Scene",
                documentName.empty() ? "Untitled" : documentName.c_str());
    ImGui::PopStyleColor();
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
    
    m_visitedEntities.clear();
    m_pendingReparents.clear();
    m_pendingDestructions.clear();
    for (Entity* entity : m_rootEntities) {
        DrawEntityNode(entity);
    }

    if (m_selectedEntity &&
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        m_selectedEntity = nullptr;
        g_UIManager->setEntity(nullptr);
    }

    const ImVec2 emptySpace = ImGui::GetContentRegionAvail();
    if (emptySpace.x > 0.0f && emptySpace.y > 0.0f &&
        ImGui::InvisibleButton("##empty_hierarchy_space", emptySpace)) {
        m_selectedEntity = nullptr;
        g_UIManager->setEntity(nullptr);
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

    const auto isVisibleChild = [entity](Entity* child) {
        return child && !(entity->isPrefabRoot && child->isPrefabChild());
    };
    const bool hasVisibleChildren = std::any_of(
        entity->children.begin(), entity->children.end(), isVisibleChild);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DrawLinesToNodes;
    
    if (m_selectedEntity == entity) flags |= ImGuiTreeNodeFlags_Selected;
    if (!hasVisibleChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;

    ImGui::PushStyleColor(ImGuiCol_Text,
                          entity->isPrefabRoot
                              ? ImVec4(0.60f, 1.00f, 0.79f, 1.0f)
                              : ImVec4(0.83f, 0.83f, 0.83f, 1.0f));

    const char *name = entity->isPrefabRoot ? ICON_MDI_CUBE_OUTLINE " %s"
                                            : ICON_MDI_CUBE_OUTLINE " %s";

    // Отрисовываем узел дерева
    bool isOpened = ImGui::TreeNodeEx((void*)(uintptr_t)entity, flags, name, entity->m_name.c_str());

    ImGui::PopStyleColor();

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
            if (isVisibleChild(child)) {
                DrawEntityNode(child);
            }
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
