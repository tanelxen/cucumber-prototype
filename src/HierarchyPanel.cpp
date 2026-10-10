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

#include <imgui.h>
#include <imgui_internal.h>

#include "../vendor/tinyfiledialogs.h"

static std::vector<Entity*> rootEntities {};
static Entity* selectedEntity;

HierarchyPanel::HierarchyPanel() : EditorPanel("Hierarchy")
{
    
}

void HierarchyPanel::draw()
{
    auto activeScene = SceneManager::Get().getActiveScene();
    
    if (!activeScene)
    {
        ImGui::Begin("Hierarchy"); ImGui::Text("No active scene"); ImGui::End();
        return;
    }
    
    ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoCollapse);
    
    if (ImGui::Button("+"))
    {
        auto count = (int)activeScene->m_entities.size();
        auto entity = std::make_unique<Entity>("Entity_" + std::to_string(count));
        entity->scene = activeScene;
        
        selectedEntity = entity.get();
        g_UIManager->setEntity(selectedEntity);
        
        activeScene->m_entities.push_back(std::move(entity));
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Add Prefab"))
    {
        char const * filter[2] = { "*.json", "*.prefab" };
        const char* path = tinyfd_openFileDialog("Load Prefab", "./assets/prefabs/", 2, filter, NULL, 0);
        
        if (path != nullptr)
        {
            auto count = (int)activeScene->m_entities.size();
            
            std::filesystem::path project_root = std::filesystem::current_path();
            auto rel_path = "assets" / std::filesystem::relative(path, project_root / "assets");
            
            auto entity = SceneSerializer::InstantiatePrefab(activeScene, rel_path);
            
            if (entity)
            {
                selectedEntity = entity;
                g_UIManager->setEntity(selectedEntity);
            }
        }
    }
    
    ImGui::Separator();
    
    rootEntities.clear();
    
    for (auto& entity : activeScene->m_entities)
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
    
    if (selectedEntity == nullptr) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    
    if (ImGui::TreeNodeEx(&rootEntities, flags, "Scene"))
    {
        if (ImGui::IsItemClicked()) {
            selectedEntity = nullptr;
            g_UIManager->setEntity(selectedEntity);
        }
        
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
    bool isOpened = ImGui::TreeNodeEx((void*)(uintptr_t)entity, flags, entity->isPrefabRoot ? "[P] %s" : "%s", entity->m_name.c_str());
    
    // Умное выделение объекта с учетом, что это может быть просто drag-n-drop
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::IsItemHovered(ImGuiHoveredFlags_None))
    {
        selectedEntity = entity;
        g_UIManager->setEntity(selectedEntity);
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
            Entity* draggedEntity = *static_cast<Entity**>(payload->Data);
            
            // Защита: объект не может стать родителем самого себя
            // И не может стать ребенком своего собственного ребенка (цикл)
            if (draggedEntity && draggedEntity != entity && !IsChildOf(entity, draggedEntity))
            {
                draggedEntity->setParent(entity, true);
            }
        }
        
        ImGui::EndDragDropTarget();
    }
    
    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Удалить объект"))
        {
            selectedEntity->destroy();
            selectedEntity = nullptr;
            g_UIManager->setEntity(selectedEntity);
        }
        
        // Если этот объект на сцене пришел из префаба — даем возможность его открыть!
        if (selectedEntity && !selectedEntity->prefabAssetPath.empty() && ImGui::MenuItem("Редактировать префаб"))
        {
            // Открываем префаб, сцена уровня автоматически скрывается в бэкграунд!
            SceneManager::Get().enterPrefabEditMode(selectedEntity->prefabAssetPath);
        }
        
        if (ImGui::MenuItem("Сохранить как префаб"))
        {
            char const * filter[2] = { "*.prefab", "*.json" };
            const char* path = tinyfd_saveFileDialog("Save Prefab", "./assets/prefabs/", 2, filter, NULL);
            
            if (path != nullptr)
            {
                std::filesystem::path project_root = std::filesystem::current_path();
                auto rel_path = "assets" / std::filesystem::relative(path, project_root / "assets");
                
                auto activeScene = SceneManager::Get().getActiveScene();
                SceneSerializer::CreatePrefabFromEntity(activeScene, selectedEntity, rel_path);
            }
        }
        
        ImGui::EndPopup();
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

