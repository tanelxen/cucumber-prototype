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

struct Node
{
    std::string name;
    std::vector<std::shared_ptr<Node>> children;
    
    Node(const std::string& nodeName) : name(nodeName) {}
};

static Node* selectedNode = nullptr;
static Node root("Test Level");

void DrawSceneNode(Node* node)
{
    // Настройки флагов для текущего узла дерева
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_DrawLinesToNodes;
    
    // Если у узла нет детей, делаем его "листом" (убираем стрелочку раскрытия)
    if (node->children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_Bullet;
    }
    
    if (selectedNode == node) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    
    // Отображаем узел дерева. Идентификатором (ID) делаем указатель на объект node
    bool isNodeOpen = ImGui::TreeNodeEx((void*)node, flags, "%s", node->name.c_str());
    
    // Обработка клика мыши для выбора узла
    if (ImGui::IsItemClicked()) {
        selectedNode = node;
    }
    
    if (ImGui::BeginPopupContextItem())
    {
        selectedNode = node;
        if (ImGui::MenuItem("Удалить объект")) {
            // Логика удаления
        }
        if (ImGui::MenuItem("Создать дочерний объект")) {
            // Логика создания
        }
        ImGui::EndPopup();
    }
    
    // Если узел раскрыт и у него есть дети, рекурсивно рисуем их
    if (isNodeOpen && !node->children.empty()) {
        for (const auto& child : node->children) {
            DrawSceneNode(child.get());
        }
        ImGui::TreePop(); // Закрываем область видимости текущего уровня дерева
    }
}

static Entity* selectedEntity = nullptr;

void DrawEntity(Entity* entity)
{
    // Настройки флагов для текущего узла дерева
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_DrawLinesToNodes;
    
    // Если у узла нет детей, делаем его "листом" (убираем стрелочку раскрытия)
//    if (node->children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_Bullet;
//    }
    
    if (selectedEntity == entity) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    
    // Отображаем узел дерева. Идентификатором (ID) делаем указатель на объект node
    bool isNodeOpen = ImGui::TreeNodeEx((void*)entity, flags, "%s", entity->m_name.c_str());
    
    // Обработка клика мыши для выбора узла
    if (ImGui::IsItemClicked()) {
        selectedEntity = entity;
    }
    
    if (ImGui::BeginPopupContextItem())
    {
        selectedEntity = entity;
        
        if (ImGui::MenuItem("Удалить объект")) {
            // Логика удаления
        }
        
        if (ImGui::MenuItem("Создать дочерний объект")) {
            // Логика создания
        }
        
        ImGui::EndPopup();
    }
    
//    // Если узел раскрыт и у него есть дети, рекурсивно рисуем их
//    if (isNodeOpen && !node->children.empty()) {
//        for (const auto& child : node->children) {
//            DrawSceneNode(child.get());
//        }
//        ImGui::TreePop(); // Закрываем область видимости текущего уровня дерева
//    }
}

// Главная функция вызова в вашем цикле рендеринга ImGui
void DrawSceneTree(Node* rootNode)
{
    ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoCollapse);
    
    if (rootNode)
    {
        DrawSceneNode(rootNode);
    }
    
    ImGui::End();
}

ScenePanel::ScenePanel() : EditorPanel("Scene")
{
    
}

void ScenePanel::setScene(WorldScene *scene)
{
    m_scene = scene;
    
    auto SystemNode = std::make_shared<Node>("System");
    auto EnvironmentNode = std::make_shared<Node>("Environment");
    auto ActorsNode = std::make_shared<Node>("Actors");
    
    root.children.push_back(SystemNode);
    root.children.push_back(EnvironmentNode);
    root.children.push_back(ActorsNode);
    
    
    // 3. Заполняем системный узел (камера и свет)
    SystemNode->children.push_back(std::make_shared<Node>("Main Camera"));
    SystemNode->children.push_back(std::make_shared<Node>("Directional Light"));
    
    // 4. Заполняем окружение (статические объекты)
    EnvironmentNode->children.push_back(std::make_shared<Node>("Terrain"));
    EnvironmentNode->children.push_back(std::make_shared<Node>("Skybox"));
    
    // Создаем узел для статических мешей внутри окружения
    auto staticProps = std::make_shared<Node>("Static Props");
    staticProps->children.push_back(std::make_shared<Node>("Tree_01"));
    staticProps->children.push_back(std::make_shared<Node>("Tree_02"));
    staticProps->children.push_back(std::make_shared<Node>("Rock_Large"));
    
    EnvironmentNode->children.push_back(staticProps);
    
    // 5. Заполняем персонажей (динамические объекты)
    auto playerNode = std::make_shared<Node>("Player");
    // У игрока может быть вложенная модель оружия или точка привязки камеры
    playerNode->children.push_back(std::make_shared<Node>("M4A1_Mesh"));
    playerNode->children.push_back(std::make_shared<Node>("Camera_Target"));
    
    auto enemyNode = std::make_shared<Node>("Enemy_Zombie");
    
    ActorsNode->children.push_back(playerNode);
    ActorsNode->children.push_back(enemyNode);
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
                        m_scene->m_entities[i] = std::move(m_scene->m_entities.back());
                        m_scene->m_entities.pop_back();
                    }
                    
                    selectedIndex = -2;
                }
                
                if (ImGui::MenuItem("Создать дочерний объект")) {
                    // Логика создания
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
