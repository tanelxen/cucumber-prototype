//
//  ViewportPanel.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#include "ScenePanel.h"
#include "UIManager.h"
#include "World.h"
#include "Entity.h"

#include <glad/glad.h>
#include <imgui.h>

#include "../vendor/tinyfiledialogs.h"

ScenePanel::ScenePanel() : EditorPanel("Scene")
{
    
}

void ScenePanel::setScene(WorldScene *scene)
{
    m_scene = scene;
}

void ScenePanel::draw()
{
    static int selectedIndex = -1;
    
    ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoCollapse);
    
    if (ImGui::Button("+"))
    {
        auto count = (int)m_scene->m_entities.size();
        auto entity = std::make_unique<Entity>("Entity_" + std::to_string(count));
        
        m_scene->m_entities.push_back(std::move(entity));
        m_scene->selectEntityAtIndex(count);
        selectedIndex = count;
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Add Prefab"))
    {
        char const * filter[2] = { "*.json", "*.prefab" };
        const char* path = tinyfd_openFileDialog("Load Prefab", "./assets/prefabs/", 2, filter, NULL, 0);
        
        if (path != nullptr)
        {
            auto count = (int)m_scene->m_entities.size();
            
            Entity* root;
            m_scene->loadPrefab(root, std::filesystem::relative(path, "./"));
            m_scene->selectEntityAtIndex(count);
            selectedIndex = count;
        }
    }
    
    ImGui::Separator();
    
    
    
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DrawLinesToNodes;
    
    if (selectedIndex == -1) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    
    if (ImGui::TreeNodeEx(&m_scene, flags, "Scene"))
    {
        if (ImGui::IsItemClicked()) {
            selectedIndex = -1;
        }
        
        for (int i = 0; i < m_scene->m_entities.size(); ++i)
        {
            auto& entity = m_scene->m_entities[i];
            
            // Настройки флагов для текущего узла дерева
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DrawLinesToNodes;
            
            // Если у узла нет детей, делаем его "листом" (убираем стрелочку раскрытия)
            //    if (node->children.empty()) {
            flags |= ImGuiTreeNodeFlags_Leaf /*| ImGuiTreeNodeFlags_NoTreePushOnOpen */| ImGuiTreeNodeFlags_Bullet;
            //    }
            
            if (selectedIndex == i) {
                flags |= ImGuiTreeNodeFlags_Selected;
            }
            
            const char* name = entity->m_name.empty() ? "?" : entity->m_name.c_str();
            
            bool isOpened = ImGui::TreeNodeEx(&entity, flags, "%s", name);
            

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::IsItemHovered(ImGuiHoveredFlags_None))
            {
                Entity* picked_entity = m_scene->m_entities[i].get();
                
                if (g_UIManager->m_is_picking_mode) {
                    // Если мы в режиме пипетки, вызываем сохраненную лямбду
                    if (g_UIManager->m_active_picking_callback && picked_entity) {
                        g_UIManager->m_active_picking_callback(picked_entity);
                    }
                    
                    g_UIManager->endPicking();
                } else {
                    // Обычная логика выбора объекта для инспектора
                    //                m_selected_entity = picked_entity;
                    m_scene->selectEntityAtIndex(i);
                    selectedIndex = i;
                }
            }
            
            // =================================================================
            // НАЧАЛО МАГИИ DRAG-N-DROP (ИСТОЧНИК)
            // =================================================================
            // Проверяем, начал ли пользователь тащить мышкой этот конкретный элемент дерева
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                
                // Нам нужно передать указатель на саму сущность (Entity*).
                // Для этого мы передаем адрес указателя (&entity), а размер равен sizeof(Entity*)
                // Строка "HIERARCHY_ENTITY" — это уникальный строковый идентификатор payload,
                // он должен в точности совпадать с тем, что мы написали в инспекторе!
                ImGui::SetDragDropPayload("HIERARCHY_ENTITY", &entity, sizeof(Entity*));
                
                // Визуальный текст-подсказка, который будет лететь за курсором мыши
                ImGui::Text("Link Entity: %s", entity->m_name.c_str());
                
                ImGui::EndDragDropSource();
            }
            // =================================================================
            
            
            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Удалить объект"))
                {
                    if (i < m_scene->m_entities.size())
                    {
                        m_scene->m_entities[i]->destroy();
                    }
                    
                    g_UIManager->setEntity(nullptr);
                    selectedIndex = -2;
                }
                
//                if (ImGui::MenuItem("Создать дочерний объект")) {
//                    // Логика создания
//                }
                
                if (ImGui::MenuItem("Сохранить как префаб"))
                {
                    char const * filter[2] = { "*.prefab", "*.json" };
                    const char* path = tinyfd_saveFileDialog("Save Prefab", "./assets/prefabs/", 2, filter, NULL);
                    
                    if (path != nullptr) {
                        m_scene->saveAsPrefab(m_scene->m_entities[i].get(), path);
                    }
                }
                
                ImGui::EndPopup();
            }
            
            if (isOpened) {
                ImGui::TreePop();
            }
        }
        
        ImGui::TreePop();
    }
    
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (g_UIManager->m_is_picking_mode)
        {
            g_UIManager->endPicking();
        }
    }
    
    ImGui::End();
}
