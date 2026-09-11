//
//  HierarchyPanel.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 10.09.26.
//

#pragma once

class HierarchyPanel {
public:
    // Главный метод отрисовки окна
    void Draw(const std::vector<Entity*>& rootEntities, Entity*& selectedEntity) {
        ImGui::Begin("Hierarchy");
        
        // Отрисовываем только корневые сущности (у которых нет родителя)
        for (Entity* entity : rootEntities) {
            if (entity->parent == nullptr) {
                DrawEntityNode(entity, selectedEntity);
            }
        }
        
        // Drop-зона на пустое место панели (чтобы сделать объект снова корневым)
        ImVec2 minPos = ImGui::GetWindowPos();
        ImVec2 maxPos = ImVec2(minPos.x + ImGui::GetWindowWidth(), minPos.y + ImGui::GetWindowHeight());
        if (ImGui::BeginDragDropTargetCustom(ImRect(minPos, maxPos), ImGui::GetID("HierarchyEmptySpace"))) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
                Entity* draggedEntity = *static_cast<Entity**>(payload->Data);
                if (draggedEntity) {
                    draggedEntity->SetParent(nullptr); // Отцепляем от родителя
                }
            }
            ImGui::EndDragDropTarget();
        }
        
        ImGui::End();
    }
    
private:
    // Рекурсивный метод для отрисовки дерева объектов
    void DrawEntityNode(Entity* entity, Entity*& selectedEntity) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        
        if (selectedEntity == entity) flags |= ImGuiTreeNodeFlags_Selected;
        if (entity->children.empty())  flags |= ImGuiTreeNodeFlags_Leaf;
        
        // Отрисовываем узел дерева
        bool isOpened = ImGui::TreeNodeEx((void*)(uintptr_t)entity->id, flags, "%s", entity->name.c_str());
        
        // Клик — выделение объекта
        if (ImGui::IsItemClicked()) {
            selectedEntity = entity;
        }
        
        // --- 1. DRAG-N-DROP: ИСТОЧНИК (Тащим этот объект) ---
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("HIERARCHY_ENTITY", &entity, sizeof(Entity*));
            ImGui::Text("Move: %s", entity->name.c_str());
            ImGui::EndDragDropSource();
        }
        
        // --- 2. DRAG-N-DROP: ПРИЕМНИК (Бросаем ДРУГОЙ объект на этот) ---
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
                Entity* draggedEntity = *static_cast<Entity**>(payload->Data);
                
                // Защита: объект не может стать родителем самого себя
                // И не может стать ребенком своего собственного ребенка (цикл)
                if (draggedEntity && draggedEntity != entity && !IsChildOf(entity, draggedEntity)) {
                    draggedEntity->SetParent(entity);
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
    
    // Проверка: является ли node потенциальным ребенком parent (защита от зацикливания иерархии)
    bool IsChildOf(Entity* potentialChild, Entity* potentialParent) {
        Entity* current = potentialChild->parent;
        while (current != nullptr) {
            if (current == potentialParent) return true;
            current = current->parent;
        }
        return false;
    }
};

