//
//  ViewportPanel.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 17.07.26.
//

#include "InspectorPanel.h"
#include "UIManager.h"

#include "Mesh.h"
#include "ModelLoader.h"

#include "Entity.h"
#include "TransformComponent.h"
#include "AnimatorComponent.h"
#include "CameraComponent.h"
#include "ScriptComponent.h"
#include "AudioComponent.h"
#include "MeshComponent.h"

#include "q3/Q3BspCollision.h"
#include "MovementComponent.h"

#include "ScriptEngine.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <stdio.h>
#include <filesystem>

#include <unordered_map>
#include <typeindex>
#include <functional>

#include "../vendor/tinyfiledialogs.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../vendor/stb_image.h"

std::vector<std::string> GetAvailableScriptClasses();

extern Q3BspCollision g_collision;

class ComponentInspector
{
public:
    // Сигнатура функции отрисовки: принимает чистый указатель на компонент
    using RenderFunc = std::function<void(Component*, InspectorPanel* p)>;
    
    static ComponentInspector& Get() {
        static ComponentInspector instance;
        return instance;
    }
    
    // Регистрация нового UI для компонента
    template<typename T>
    void Register(const std::string& displayName, RenderFunc func) {
        std::type_index id = std::type_index(typeid(T));
        registry[id] = { displayName, func };
    }
    
    // Отрисовка компонента по его динамическому типу
    void Render(Component* component, InspectorPanel* panel)
    {
        if (!component) return;
        
        std::type_index id = component->GetTypeIndex();
        auto it = registry.find(id);
        
        ImGui::PushID(component);
        
        if (it != registry.end()) {
            const auto& info = it->second;
            
            // Отрисовываем красивый заголовок в ImGui
            if (ImGui::CollapsingHeader(info.displayName.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                // Вызываем кастомную функцию отрисовки
                info.renderFunc(component, panel);
            }
        } else {
            // Фолбэк, если UI для компонента забыли зарегистрировать
            ImGui::CollapsingHeader(typeid(*component).name());
        }
        
        ImGui::PopID();
    }
    
    // Дополнительный метод для получения красивого имени (для меню добавления)
    std::string GetName(std::type_index id) {
        auto it = registry.find(id);
        return (it != registry.end()) ? it->second.displayName : id.name();
    }
    
private:
    struct InspectorInfo {
        std::string displayName;
        RenderFunc renderFunc;
    };
    
    std::unordered_map<std::type_index, InspectorInfo> registry;
};

static void RegisterUI();

void InspectorPanel::setEntity(Entity *entity)
{
    m_entity = entity;
    RegisterUI();
}

void DrawTransformComponent(TransformComponent* transform);
void DrawAudioComponentInspector(AudioComponent* component);
void DrawScriptProperties(asITypeInfo* type);

static void RegisterUI()
{
    auto& inspector = ComponentInspector::Get();
    
    inspector.Register<TransformComponent>("Transform", [](Component* c, InspectorPanel* p) {
        auto* comp = static_cast<TransformComponent*>(c);
        DrawTransformComponent(comp);
    });
    
    inspector.Register<MeshComponent>("Mesh", [](Component* c, InspectorPanel* p) {
        auto* comp = static_cast<MeshComponent*>(c);
        
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "%s", comp->path.c_str());
        
        ImGui::InputText("##Path", buffer, 256, ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        if (ImGui::Button("..."))
        {
            char const * filter[2] = { "*.gltf", "*.glb" };
            const char* path = tinyfd_openFileDialog(NULL, "./assets/meshes/gltf/", 2, filter, NULL, 0);
            
            if (path != nullptr)
            {
                auto resource = ModelLoader::LoadModel(path);
                
                comp->mesh = resource.mesh;
                comp->materials = resource.embeddedMaterials;
                
                if (resource.hasSkeleton())
                {
                    auto animator = comp->owner->getComponent<AnimatorComponent>();
                    
                    if (animator == nullptr) {
                        animator = comp->owner->addComponent<AnimatorComponent>();
                    }
                    
                    animator->setSkeleton(resource.skeleton);
                    animator->m_clips = resource.animations;
                    animator->m_currentClipIndex = -1;
                }
                
                
                std::filesystem::path base_path("./assets");
                auto rel_path = std::filesystem::relative(path, base_path);
                comp->path = rel_path;
            }
        }
        
        if (comp->mesh) {
            ImGui::Checkbox("Skybox ambient", &comp->mesh->m_useAmbientLight);
        }
        
        if (comp->mesh && !comp->mesh->m_surfaces.empty())
        {
            if (ImGui::TreeNode("Surfaces"))
            {
                for (int i = 0; i < comp->mesh->m_surfaces.size(); i++)
                {
                    if (ImGui::TreeNode((void*)(intptr_t)i, "Surface %i", i))
                    {
                        auto& surface = comp->mesh->m_surfaces[i];
                        ImGui::Text("%s: %i tris, %i material", surface.m_name.c_str(), surface.count / 3, surface.materialSlot);
                        
                        auto& material = comp->mesh->m_materials[surface.materialSlot];
                        
                        if (ImGui::Button("Edit material"))
                        {
                            p->m_manager->setMaterial(&material);
                        }
                        
                        ImGui::TreePop();
                    }
                }
                
                ImGui::TreePop();
            }
        }
    });
    
    inspector.Register<AnimatorComponent>("Animator", [](Component* c, InspectorPanel* p) {
        auto* comp = static_cast<AnimatorComponent*>(c);
        if (!comp->m_clips.empty())
        {
            auto& clips = comp->m_clips;
            
            const char* title = comp->m_currentClipIndex == -1 ? "<no selected>" : clips[comp->m_currentClipIndex]->name.c_str();
            
            if (ImGui::BeginCombo("##sequence combo", title, ImGuiComboFlags_None))
            {
                for (int i = 0; i < clips.size(); ++i)
                {
                    bool is_selected = (comp->m_currentClipIndex == i);
                    
                    if (ImGui::Selectable(clips[i]->name.c_str(), is_selected))
                    {
                        comp->play(i);
                    }
                    
                    if (is_selected) ImGui::SetItemDefaultFocus();
                }
                
                ImGui::EndCombo();
            }
        }
    });
    
    inspector.Register<AudioComponent>("Sound", [](Component* c, InspectorPanel* p) {
        auto* comp = static_cast<AudioComponent*>(c);

        char buffer[256];
        snprintf(buffer, sizeof(buffer), "%s", comp->path.c_str());
        
        ImGui::InputText("##Path", buffer, 256, ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        if (ImGui::Button("..."))
        {
            char const * filter[2] = { "*.mp3", "*.wav" };
            const char* path = tinyfd_openFileDialog(NULL, "./assets/sounds/", 2, filter, NULL, 0);
            
            if (path != nullptr)
            {
                comp->init(path);
                
                std::filesystem::path base_path("./assets");
                auto rel_path = std::filesystem::relative(path, base_path);
                comp->path = rel_path;
            }
        }
        
        if (comp->m_isInitialized)
        {
            DrawAudioComponentInspector(comp);
        }
    });
    
    inspector.Register<ScriptComponent>("Script", [](Component* c, InspectorPanel* p) {
        auto* comp = static_cast<ScriptComponent*>(c);
        
        ImGui::TextDisabled("%s", comp->scriptPath.c_str());
        
        if (comp->m_isInitialized)
        {
            if (ImGui::Button("Edit")) {
                
#if defined(_WIN32)
                // Для Windows: открывает файл в программе по умолчанию (обычно Блокнот)
                std::string command = "start \"" + comp->scriptPath + "\"";
#elif defined(__APPLE__)
                // Для macOS
                std::string command = "open \"" + comp->scriptPath + "\"";
#else
                // Для Linux
                std::string command = "xdg-open \"" + comp->scriptPath + "\"";
#endif
                
                std::system(command.c_str());
            }
            
            comp->drawProperties();
        }
    });
}

void InspectorPanel::draw()
{
    ImGui::Begin("Inspector");
    
    if (!m_entity)
    {
        ImGui::End();
        return;
    }
    
    ImGui::Checkbox("##active", &m_entity->m_isActive);
    
    ImGui::SameLine();
    
    char buffer[256] = "";
    snprintf(buffer, sizeof(buffer), "%s", m_entity->m_name.c_str());
    
    if (ImGui::InputText("##Name", buffer, sizeof(buffer)))
    {
        auto& name = m_entity->m_name;
        name = buffer;
    }
    
    ImGui::TextDisabled("uuid: %s", m_entity->uid.toText().c_str());
    
    ImGui::Separator();
    
    ComponentInspector::Get().Render(&m_entity->getTransform(), this);
    ImGui::Spacing();
    
    for (auto& component : m_entity->getComponents())
    {
        ComponentInspector::Get().Render(component.get(), this);
        ImGui::Spacing();
    }
    
    ImGui::Separator();
    ImGui::Spacing();
    
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentPopup");
    }
    
    if (ImGui::BeginPopup("AddComponentPopup"))
    {
//        if (ImGui::MenuItem("Transform"))   { m_entity->addComponent<TransformComponent>(); }
        if (ImGui::MenuItem("Mesh"))        { m_entity->addComponent<MeshComponent>(); }
        if (ImGui::MenuItem("Sound"))       { m_entity->addComponent<AudioComponent>(); }
        if (ImGui::MenuItem("Camera"))      { m_entity->addComponent<CameraComponent>(); }
        if (ImGui::MenuItem("Movement"))    { m_entity->addComponent<MovementComponent>(&g_collision); }
        
        if (ImGui::BeginMenu("Scripts")) {
            // Получаем актуальный список классов из AngelScript
            auto availableScripts = GetAvailableScriptClasses();
            
            if (availableScripts.empty()) {
                ImGui::TextDisabled("No scripts found");
            }
            
            for (const auto& className : availableScripts)
            {
                if (ImGui::MenuItem(className.c_str()))
                {
                    auto comp = m_entity->addComponent<ScriptComponent>();
                    comp->init(className);
                    
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndMenu();
        }
        
        ImGui::EndPopup();
    }
    
    ImGui::End();
//    ImGui::PopStyleVar();
}

InspectorPanel::InspectorPanel() : EditorPanel("Inspector")
{
    
}

bool DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue, float columnWidth)
{
    ImGuiIO& io = ImGui::GetIO();
    auto boldFont = io.Fonts->Fonts[0];
    
    ImGui::PushID(label.c_str());
    
    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, columnWidth);
    ImGui::Text("%s", label.c_str());
    ImGui::NextColumn();
    
    ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });
    
    float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
    ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };
    
    bool isChangedX = false;
    bool isChangedY = false;
    bool isChangedZ = false;
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.20f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
    
    ImGui::PushFont(boldFont);
    if (ImGui::Button("X", buttonSize)) {
        values.x = resetValue;
        isChangedX = true;
    }
    ImGui::PopFont();
    
    ImGui::PopStyleColor(3);
    
    ImGui::SameLine();
    if (ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f")) {
        isChangedX = true;
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
    
    ImGui::PushFont(boldFont);
    if (ImGui::Button("Y", buttonSize)) {
        values.y = resetValue;
        isChangedY = true;
    }
    ImGui::PopFont();
    
    ImGui::PopStyleColor(3);
    
    ImGui::SameLine();
    if (ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f")) {
        isChangedY = true;
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
    
    ImGui::PushFont(boldFont);
    if (ImGui::Button("Z", buttonSize)) {
        values.z = resetValue;
        isChangedZ = true;
    }
    ImGui::PopFont();
    
    ImGui::PopStyleColor(3);
    
    ImGui::SameLine();
    if (ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f")) {
        isChangedZ = true;
    }
    
    ImGui::PopItemWidth();
    
    ImGui::PopStyleVar();
    
    ImGui::Columns(1);
    
    ImGui::PopID();
    
    return isChangedX || isChangedY || isChangedZ;
}

void DrawTransformComponent(TransformComponent* transform)
{
    // 1. ПОЗИЦИЯ
    glm::vec3 pos = transform->getPosition();
    // Передаем копию. Если пользователь покрутил ползунок, функция вернет true
    if (DrawVec3Control("Position", pos, 0.0f, 100.0f)) {
        transform->setPosition(pos); // Срабатывает сеттер и инвалидация!
    }
    
    // 2. ПОВОРОТ (Скрываем кватернион, показываем углы Эйлера)
    // Извлекаем углы из трансформера (или используем сохраненный m_eulerDegrees, как обсуждали ранее)
    glm::vec3 eulerAngles = transform->getRotation();
    
    if (DrawVec3Control("Rotation", eulerAngles, 0.0f, 100.0f)) {
        transform->setRotation(eulerAngles); // Сеттер переведет в кватернион и сбросит кэш
    }
    
    // 3. МАСШТАБ
    glm::vec3 scale = transform->getScale();
    if (DrawVec3Control("Scale", scale, 1.0f, 100.0f)) {
        transform->setScale(scale);
    }
}

void DrawAudioComponentInspector(AudioComponent* component)
{
    // 1. Управление воспроизведением (Кнопки Play / Pause / Stop в один ряд)
    if (component->IsPlaying()) {
        // Если играет, показываем кнопку Pause (в miniaudio это просто Stop)
        if (ImGui::Button("Pause##SoundPause")) {
            component->stop();
        }
    } else {
        if (ImGui::Button("Play##SoundPlay")) {
            component->play();
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Stop##SoundStop")) {
        component->stop();
        component->SetCurrentTime(0.0f); // Сбрасываем в начало
    }
    
    ImGui::SameLine();
    bool isLooping = component->IsLooping();
    if (ImGui::Checkbox("Looping##SoundLooping", &isLooping)) {
        component->setLooping(isLooping);
    }
    
//    // 2. Прогресс-бар воспроизведения
    float currentTime = component->GetCurrentTime();
    float totalTime = component->m_length;
    float progress = (totalTime > 0.0f) ? (currentTime / totalTime) : 0.0f;
    
    // Форматируем текст времени: "01:23 / 03:45"
    char timeBuffer[64];
    snprintf(timeBuffer, sizeof(timeBuffer), "%.1fs / %.1fs", currentTime, totalTime);
    
    // Отрисовка прогресс-бара
    ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f), timeBuffer);
    
//    // Интерактивная перемотка (Slider), если хочется кликать по таймлайну
//    float seekTime = currentTime;
//    if (ImGui::SliderFloat("Seek", &seekTime, 0.0f, totalTime, " ")) {
//        component->SetCurrentTime(seekTime);
//    }
//    
//    // 3. Основные настройки звука
//    // Настройки громкости
//    float volume = component->GetVolume();
//    if (ImGui::SliderFloat("Volume", &volume, 0.0f, 2.0f, "%.2f")) {
//        component->setVolume(volume);
//    }
}

// Функция возвращает имена всех скриптовых классов, наследующих MonoBehaviour
std::vector<std::string> GetAvailableScriptClasses()
{
    std::vector<std::string> scriptClasses;
    
    // Получаем наш единый модуль
    asIScriptModule* mod = ScriptEngine::get().GetRawEngine()->GetModule("Game", asGM_ONLY_IF_EXISTS);
    if (!mod) return scriptClasses;
    
    // Находим базовый тип MonoBehaviour для проверки наследования
    asITypeInfo* baseType = mod->GetTypeInfoByName("MonoBehaviour");
    if (!baseType) return scriptClasses;
    
    // Обходим все типы (классы), объявленные в модуле
    asUINT typeCount = mod->GetObjectTypeCount();
    
    for (asUINT i = 0; i < typeCount; i++)
    {
        asITypeInfo* type = mod->GetObjectTypeByIndex(i);
        if (!type) continue;
        
        // Нам нужны только классы (не enums/интерфейсы) и они не должны быть абстрактными
        if (type->GetFlags() & asOBJ_SCRIPT_OBJECT && !(type->GetFlags() & asOBJ_ABSTRACT))
        {
            
            // Проверяем, наследуется ли этот класс от MonoBehaviour
            if (type->DerivesFrom(baseType)) {
                scriptClasses.push_back(type->GetName());
            }
        }
    }
    
    return scriptClasses;
}
