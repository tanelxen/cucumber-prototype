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
#include "CollisionComponent.h"
#include "MovementComponent.h"
#include "AnimatorComponent.h"
#include "CameraComponent.h"
#include "ScriptComponent.h"
#include "AudioComponent.h"
#include "MeshComponent.h"

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
static void DrawCollisionComponent(CollisionComponent* comp);

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
                
                std::filesystem::path base_path("./assets");
                auto rel_path = std::filesystem::relative(path, base_path);
                comp->path = rel_path;
                
                if (resource.hasSkeleton())
                {
                    auto animator = comp->owner->getComponent<AnimatorComponent>();
                    
                    if (animator == nullptr) {
                        animator = comp->owner->addComponent<AnimatorComponent>();
                    }
                    
                    animator->setSkeleton(resource.skeleton);
                    animator->m_clips = resource.animations;
                    animator->m_currentClipIndex = -1;
                    animator->path = rel_path;
                }
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
    
    inspector.Register<MovementComponent>("Movement", [](Component* c, InspectorPanel* p) {
        auto* comp = static_cast<MovementComponent*>(c);
        ImGui::DragFloat("Walk Speed##Movement", &comp->m_forwardspeed, 10.f, 0.0f, 1000.0f, "%.0f");
    });
    
    inspector.Register<CollisionComponent>("Collision", [](Component* c, InspectorPanel* p) {
        auto* comp = static_cast<CollisionComponent*>(c);
        DrawCollisionComponent(comp);
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
    
    DrawInspector(m_entity);
    
    ImGui::End();
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


static void DrawCollisionBits(const char* label, uint32_t& bitmask)
{
    ImGui::Text("%s", label);
    ImGui::PushID(label);
    
    // Сетка 4х4 для 16 слоев (если нужно 32, сделайте 8х4 или 4х8)
    const int numRows = 2;
    const int numCols = 8;
    
    // Делаем кнопки маленькими и квадратными
    ImVec2 buttonSize = ImVec2(25, 25);
    
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
    
    for (int row = 0; row < numRows; ++row)
    {
        for (int col = 0; col < numCols; ++col)
        {
            int bitIndex = row * numCols + col;
            uint32_t bitValue = (1 << bitIndex);
            bool isSet = (bitmask & bitValue) != 0;
            
            // Если бит активен — подсвечиваем кнопку
            if (isSet)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.25f, 0.14f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.33f, 0.18f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.05f, 0.18f, 0.10f, 1.0f));
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.05f, 0.05f, 0.05f, 1.0f));
            }
            
            // Название кнопки — просто индекс бита (от 1 до 16 для удобства человека)
            char buf[4];
            snprintf(buf, sizeof(buf), "%d", bitIndex);
            
            ImGui::PushID(bitIndex);
            if (ImGui::Button(buf, buttonSize))
            {
                if (isSet) {
                    bitmask &= ~bitValue; // Выключаем бит
                } else {
                    bitmask |= bitValue;  // Включаем бит
                }
            }
            
            // Всплывающая подсказка при наведении на бит
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Bit %d (Value: %u)", bitIndex, bitValue);
            }
            ImGui::PopID();
            
            ImGui::PopStyleColor(3);
            
            // Если это не последняя колонка — рисуем на той же строке
            if (col < numCols - 1) {
                ImGui::SameLine();
            }
        }
    }
    
    ImGui::PopStyleVar();
    ImGui::PopID();
    ImGui::Spacing();
}

static void DrawCollisionComponent(CollisionComponent* comp)
{
//    ImGui::DragFloat3("Mins", &comp->m_mins.x, 1.f);
//    ImGui::DragFloat3("Maxs", &comp->m_maxs.x, 1.f);
    
    DrawVec3Control("Min Bounds", comp->m_mins, -16.0f, 100.0f);
    DrawVec3Control("Max Bounds", comp->m_maxs,  16.0f, 100.0f);
    
    ImGui::Separator();
    
    DrawCollisionBits("Layer", comp->layer);
//    DrawCollisionBits("Mask (С чем сталкивается)", comp->collisionMask);
    
    ImGui::Separator();
    
    ImGui::Checkbox("Trigger", &comp->isTrigger);
}

// Универсальный метод отрисовки поля ввода в зависимости от содержимого JSON
static bool DrawExposedWidget(const std::string& label, nlohmann::json& ioValue)
{
    bool changed = false;
    
    // 1. Если это число с плавающей точкой (float)
    if (ioValue.is_number_float())
    {
        float val = ioValue.get<float>();
        if (ImGui::DragFloat(label.c_str(), &val, 0.1f)) {
            ioValue = val;
            changed = true;
        }
    }
    // 2. Если это целое число (int)
    else if (ioValue.is_number_integer())
    {
        int val = ioValue.get<int>();
        if (ImGui::DragInt(label.c_str(), &val, 1)) {
            ioValue = val;
            changed = true;
        }
    }
    // 3. Если это логический тип (bool / галочка)
    else if (ioValue.is_boolean())
    {
        bool val = ioValue.get<bool>();
        if (ImGui::Checkbox(label.c_str(), &val)) {
            ioValue = val;
            changed = true;
        }
    }
    // 4. Если это строка (std::string / ввод текста)
    else if (ioValue.is_string())
    {
        std::string val = ioValue.get<std::string>();
        char buf[256];
        strncpy(buf, val.c_str(), sizeof(buf));
        if (ImGui::InputText(label.c_str(), buf, sizeof(buf))) {
            ioValue = std::string(buf);
            changed = true;
        }
    }
    // 5. Если это массив из 3-х чисел (например, glm::vec3 для Position/Rotation)
    else if (ioValue.is_array() && ioValue.size() == 3 && ioValue[0].is_number())
    {
        float vec[3] = { ioValue[0].get<float>(), ioValue[1].get<float>(), ioValue[2].get<float>() };
        if (ImGui::DragFloat3(label.c_str(), vec, 0.1f)) {
            ioValue = { vec[0], vec[1], vec[2] };
            changed = true;
        }
    }
    
    return changed;
}

static void saveCurrentPrefabInterface(Entity* prefabRoot);

static Entity* m_editingPrefabRoot = nullptr;

static bool isInPrefabEditMode() { return m_editingPrefabRoot != nullptr; }
static Entity* getEditingPrefabRoot() { return m_editingPrefabRoot; }

static void togglePrefabEditMode(Entity* entity)
{
    if (m_editingPrefabRoot == entity) m_editingPrefabRoot = nullptr; // Выйти
    else if (entity->isPrefabRoot) m_editingPrefabRoot = entity;      // Войти
}

static void DrawPrefabInterfaceEditor(Entity* prefabRoot)
{
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "--- PREFAB EDIT MODE ---");
    if (ImGui::Button("Save Changes to .prefab File")) {
        // Логика функции Apply Changes (перезапись исходного .prefab)
        saveCurrentPrefabInterface(prefabRoot);
    }
    ImGui::SameLine();
    if (ImGui::Button("Exit Edit Mode")) {
        togglePrefabEditMode(prefabRoot);
        return;
    }
    ImGui::Separator();
    
    // Таблица для удобного редактирования строк
    if (ImGui::BeginTable("InterfaceTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Source");
        ImGui::TableSetupColumn("Link");
        ImGui::TableSetupColumn("Actions");
        ImGui::TableHeadersRow();
        
        int propIdx = 0;
        // Работаем НАПРЯМУЮ с кэшем/вектором структуры инстанса
        for (auto it = prefabRoot->m_exposedInterfaceCache.begin(); it != prefabRoot->m_exposedInterfaceCache.end(); )
        {
            ImGui::TableNextRow();
            
            // 1. Колонка: Редактирование Внешнего Имени (Display Name)
            ImGui::TableSetColumnIndex(0);
            char nameBuf[64];
            strncpy(nameBuf, it->displayName.c_str(), sizeof(nameBuf));
            ImGui::PushID(propIdx);
            if (ImGui::InputText("##dispName", nameBuf, sizeof(nameBuf))) {
                it->displayName = std::string(nameBuf);
            }
            
            // 2. Колонка: Визуальное отображение куда ведет связь
            ImGui::TableSetColumnIndex(1);
            if (it->targetComponent) {
                std::string path = it->targetComponent->owner->getPathRelativeTo(prefabRoot);
                ImGui::Text("%s -> %s::%s", path.c_str(), it->targetComponent->getTypeName().c_str(), it->propertyName.c_str());
                //if (!it->subProperty.empty()) ImGui::SameLine(); ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), " (%s)", it->subProperty.c_str());
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Unlinked!");
            }
            
            // 3. Колонка: Кнопка связывания (Link Menu)
            ImGui::TableSetColumnIndex(2);
            if (ImGui::Button("Link...")) {
                ImGui::OpenPopup("LinkBindingPopup");
            }
            
            // РЕКУРСИВНОЕ КОНТЕНСТНОЕ МЕНЮ СВЯЗЫВАНИЯ
            if (ImGui::BeginPopup("LinkBindingPopup"))
            {
                ImGui::Text("Select Target Component Property:");
                ImGui::Separator();
                
                // Лямбда для обхода дерева префаба и генерации вложенных подменю ImGui
                std::function<void(Entity*)> drawEntityMenu = [&](Entity* ent) {
                    std::string entLabel = ent->getName();
                    if (ent == prefabRoot) entLabel += " (Root)";
                    
                    if (ImGui::BeginMenu(entLabel.c_str()))
                    {
                        // Сначала выводим компоненты этой сущности
                        
                        if (ImGui::BeginMenu("Transform"))
                        {
                            auto& comp = ent->getTransform();
                            
                            if (ImGui::MenuItem("Position")) {
                                it->targetComponent = &comp;
                                it->propertyName = "position";
                            }
                            if (ImGui::MenuItem("Rotation")) {
                                it->targetComponent = &comp;
                                it->propertyName = "rotation";
                            }
                            if (ImGui::MenuItem("Scale")) {
                                it->targetComponent = &comp;
                                it->propertyName = "scale";
                            }
                            
                            ImGui::EndMenu();
                        }
                        
                        /*
                        for (auto& comp : ent->getComponents())
                        {
                            if (ImGui::BeginMenu(comp->getTypeName().c_str()))
                            {
                                if (comp->getTypeName() == "TransformComponent")
                                {

                                }
                                
                                ImGui::EndMenu();
                            }
                        }
                         */
                        
                        // Затем рекурсивно спускаемся к дочерним сущностям в подменю
                        for (auto* child : ent->getChildren()) {
                            drawEntityMenu(child);
                        }
                        
                        ImGui::EndMenu();
                    }
                };
                
                drawEntityMenu(prefabRoot); // Запуск меню от корня префаба
                ImGui::EndPopup();
            }
            
            // 4. Колонка: Удаление свойства из интерфейса
            ImGui::TableSetColumnIndex(3);
            if (ImGui::Button("Delete")) {
                it = prefabRoot->m_exposedInterfaceCache.erase(it); // Удаляем свойство из таблицы
            } else {
                ++it;
            }
            
            ImGui::PopID();
            propIdx++;
        }
        ImGui::EndTable();
    }
    
    if (ImGui::Button("Add New Exposed Property"))
    {
        CachedExposedProperty newProp;
        newProp.displayName = "New Property";
        prefabRoot->m_exposedInterfaceCache.push_back(newProp);
    }
}

void InspectorPanel::DrawNormalEntityComponents(Entity* entity)
{
    ImGui::Checkbox("##active", &m_entity->m_isActive);
    
    ImGui::SameLine();
    
    char buffer[256] = "";
    snprintf(buffer, sizeof(buffer), "%s", m_entity->m_name.c_str());
    
    if (ImGui::InputText("##Name", buffer, sizeof(buffer)))
    {
        auto& name = m_entity->m_name;
        name = buffer;
    }
    
    ImGui::TextDisabled("%s", m_entity->uid.toText().c_str());
    
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
        if (ImGui::MenuItem("Movement"))    { m_entity->addComponent<MovementComponent>(); }
        if (ImGui::MenuItem("Collision"))    { m_entity->addComponent<CollisionComponent>(); }
        
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
}

void InspectorPanel::DrawPrefabInstanceInspector(Entity* prefabRoot)
{
    if (!prefabRoot || !prefabRoot->isPrefabRoot) return;
    
    //    ImGui::Text("Prefab Instance: %s", prefabRoot->getName().c_str());
    ImGui::TextDisabled("Source: %s", prefabRoot->prefabAssetPath.c_str());
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::Text("Exposed Interface (Properties):");
    ImGui::Spacing();
    
    // Запускаем плоский цикл по нашему оптимизированному кэшу указателей!
    for (auto& prop : prefabRoot->m_exposedInterfaceCache)
    {
        if (!prop.targetComponent) continue;
        
        // 1. Получаем текущее геймплейное значение переменной из компонента
        nlohmann::json currentValue = prop.targetComponent->getExposedValue(prop.propertyName);
        
        // 2. Проверяем статус оверрайда на сцене для визуального выделения
        bool isOverridden = prefabRoot->isInterfaceOverrideDirty(prop.displayName);
        
        // 3. Если значение изменено на сцене — включаем жирный шрифт (стиль Unity)
        if (isOverridden)
        {
            // Предполагается, что у вас загружен жирный шрифт в ImGui, например, вторым в списке
            // Если шрифта нет, можно просто подсветить текст цветом (ImGui::PushStyleColor)
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
        }
        
        // Строим уникальный ID виджета для ImGui, чтобы избежать конфликтов имен
        std::string widgetLabel = "##" + prop.displayName;
        
        // Рисуем имя свойства слева, сам виджет справа
        ImGui::Columns(2, nullptr, false);
        ImGui::SetColumnWidth(0, 150.0f); // Фиксированная ширина под названия свойств
        
//        if (isOverridden) {
//            // Подсвечиваем измененный текст светло-зеленым/голубым цветом
//            ImGui::TextColored(ImVec4(0.3f, 0.85f, 0.3f, 1.0f), "%s *", prop.displayName.c_str());
//        } else {
            ImGui::Text("%s", prop.displayName.c_str());
//        }
        
        ImGui::NextColumn();
        ImGui::PushItemWidth(-1.0f); // Растянуть виджет на всю ширину колонки
        
        // 4. Отрисовываем сам элемент ввода с помощью нашего хелпера
        if (DrawExposedWidget(widgetLabel, currentValue))
        {
            // Пользователь изменил значение: пушим его обратно напрямую в память компонента по указателю!
            prop.targetComponent->setExposedValue(prop.propertyName, currentValue);
            
            // Маркируем для сцены, что этот оверрайд активен
            prefabRoot->markInterfaceOverrideDirty(prop.displayName, true);
        }
        
        ImGui::PopItemWidth();
        
        // 5. Контекстное меню по правой кнопке мыши (Сброс значения / Revert)
        if (isOverridden)
        {
            ImGui::PopFont(); // Отключаем жирный шрифт перед отрисовкой меню
            
//            // Если кликнули правой кнопкой по свойству
//            if (ImGui::BeginPopupContextItem(prop.displayName.c_str()))
//            {
//                if (ImGui::MenuItem("Revert to Prefab Default"))
//                {
//                    // Читаем дефолтное значение из исходного файла префаба
//                    // (Для максимальной скорости можно кэшировать и дефолты, но для UI считывание раз в клик не критично)
//                    nlohmann::json prefabJson;
//                    std::ifstream file(prefabRoot->prefabAssetPath);
//                    file >> prefabJson;
//
//                    // Находим базовое значение сущности, у которой parent == "" (это корень префаба)
//                    nlohmann::json entities = prefabJson.is_array() ? prefabJson : prefabJson["entities"];
//                    nlohmann::json rootEntityJson;
//                    for (const auto& ent : entities) {
//                        if (!ent.contains("parent") || ent["parent"].get<std::string>().empty() || ent["parent"] == "") {
//                            rootEntityJson = ent;
//                            break;
//                        }
//                    }
//
//                    // Ищем нужный компонент в исходном файле префаба по типу
//                    nlohmann::json defaultVal;
//                    if (rootEntityJson.contains("components")) {
//                        for (const auto& comp : rootEntityJson["components"]) {
//                            if (comp["type"] == prop.targetComponent->getTypeName()) {
//                                if (comp["data"].contains(prop.propertyName)) {
//                                    defaultVal = comp["data"][prop.propertyName];
//                                }
//                                break;
//                            }
//                        }
//                    }
//
//                    // Если нашли дефолтное значение — возвращаем его
//                    if (!defaultVal.empty()) {
//                        prop.targetComponent->setExposedValue(prop.propertyName, defaultVal);
//                        prefabRoot->markInterfaceOverrideDirty(prop.displayName, false); // Снимаем флаг оверрайда!
//                    }
//                }
//                ImGui::EndPopup();
//            }
        }
        else if (isOverridden)
        {
            ImGui::PopFont(); // Обязательный баланс PopFont, если меню не открывалось
        }
        
        ImGui::Columns(1); // Сброс колонок для следующей строки
        ImGui::Spacing();
    }
}

void InspectorPanel::DrawInspector(Entity* selectedEntity)
{
    if (!selectedEntity) return;
    
    // Если это префаб, даем кнопку переключения режима
    if (selectedEntity->isPrefabRoot && !isInPrefabEditMode())
    {
        if (ImGui::Button("Edit Prefab Interface")) {
            togglePrefabEditMode(selectedEntity);
        }
        ImGui::Separator();
    }
    
    // Отрисовка в зависимости от режима
    if (selectedEntity->isPrefabRoot && m_editingPrefabRoot == selectedEntity)
    {
        // РЕЖИМ 1: Настройка внешнего интерфейса (экспорта) префаба
        DrawPrefabInterfaceEditor(selectedEntity);
    }
    else if (selectedEntity->isPrefabRoot)
    {
        // РЕЖИМ 2: Дизайнер крутит готовые ползунки на сцене (наш прошлый шаг)
        DrawPrefabInstanceInspector(selectedEntity);
    }
    else
    {
        // РЕЖИМ 3: Обычный инспектор компонентов для уникального объекта сцены
        DrawNormalEntityComponents(selectedEntity);
    }
}

#include <fstream>
#include <iostream>

static void saveCurrentPrefabInterface(Entity* prefabRoot)
{
    if (!prefabRoot || !prefabRoot->isPrefabRoot || prefabRoot->prefabAssetPath.empty()) {
        std::cerr << "[Engine Editor] Ошибка: Неверный корень префаба для сохранения интерфейса." << std::endl;
        return;
    }
    
    // 1. Сначала читаем существующий файл префаба, чтобы сохранить структуру его сущностей (entities)
    nlohmann::json prefabJson;
    {
        std::ifstream fileIn(prefabRoot->prefabAssetPath);
        if (!fileIn.is_open()) {
            std::cerr << "[Engine Editor] Не удалось открыть исходный файл префаба для чтения: "
            << prefabRoot->prefabAssetPath << std::endl;
            return;
        }
        try {
            fileIn >> prefabJson;
        } catch (const nlohmann::json::parse_error& e) {
            std::cerr << "[Engine Editor] Ошибка парсинга JSON префаба при обновлении интерфейса: "
            << e.what() << std::endl;
            return;
        }
    }
    
    // 2. Генерируем НОВЫЙ блок exposed_properties на основе нашего кэша из ImGui-редактора
    auto newExposedArray = nlohmann::json::array();
    
    for (const auto& cachedProp : prefabRoot->m_exposedInterfaceCache)
    {
        // Пропускаем нелинкованные свойства (у которых нет целевого компонента)
        if (!cachedProp.targetComponent) continue;
        
        nlohmann::json propJson;
        propJson["display_name"] = cachedProp.displayName;
        propJson["component_type"] = cachedProp.targetComponent->getTypeName();
        propJson["property_name"] = cachedProp.propertyName;
        
        // Вычисляем NodePath целевой сущности относительно корня префаба
        Entity* targetEntity = cachedProp.targetComponent->owner;
        std::string relativePath = targetEntity->getPathRelativeTo(prefabRoot);
        propJson["target_path"] = relativePath;
        
        newExposedArray.push_back(propJson);
    }
    
    // 3. Обновляем JSON структуру
    prefabJson["exposed_properties"] = newExposedArray;
    
    // Нюанс: Если пользователь также изменил базовые (дефолтные) значения свойств самого префаба
    // во время редактирования "по месту", мы можем обновить и массив "entities" в файле.
    // Для этого просто сериализуем текущее дерево в плоский список, как мы делали при создании префаба.
    auto entitiesArray = nlohmann::json::array();
    std::function<void(const Entity*)> serializeFlat = [&](const Entity* ent) {
        nlohmann::json entJson;
        entJson["name"] = ent->getName();
        entJson["parent"] = (ent == prefabRoot) ? "" : ent->getParent()->getPathRelativeTo(prefabRoot);
        
        auto componentsArray = nlohmann::json::array();
        for (auto& comp : ent->getComponents()) {
            nlohmann::json compNode;
            compNode["type"] = comp->getTypeName();
            nlohmann::json compData = nlohmann::json::object();
            
            // ВАЖНО: Мы сохраняем префаб как класс (шаблон), поэтому пишем ВСЕ свойства,
            // но перед этим временно синхронизируем m_prefabValue = m_value, так как это новые дефолты!
            comp->save(compData);
            
            compNode["data"] = compData;
            componentsArray.push_back(compNode);
        }
        entJson["components"] = componentsArray;
        entitiesArray.push_back(entJson);
        
        for (auto* child : ent->getChildren()) {
            serializeFlat(child);
        }
    };
    
    serializeFlat(prefabRoot);
    prefabJson["entities"] = entitiesArray;
    
    // 4. Записываем обновленный префаб обратно на диск
    std::ofstream fileOut(prefabRoot->prefabAssetPath);
    if (!fileOut.is_open()) {
        std::cerr << "[Engine Editor] Не удалось открыть файл префаба для записи: "
        << prefabRoot->prefabAssetPath << std::endl;
        return;
    }
    
    fileOut << prefabJson.dump(4);
    fileOut.close();
    
    /*
    // 5. Актуализируем рантайм-состояние: так как мы переписали дефолты префаба,
    // текущие значения на сцене теперь являются эталоном. Сбрасываем флаги оверрайдов сцены!
    for (auto& cachedProp : prefabRoot->m_exposedInterfaceCache)
    {
        if (cachedProp.targetComponent) {
            // Для всех свойств внутри компонентов нужно обновить m_prefabValue = m_value.
            // Простейший способ — сымитировать setFromPrefab текущим значением.
            nlohmann::json currentVal = cachedProp.targetComponent->getExposedValue(cachedProp.propertyName);
            cachedProp.targetComponent->setExposedValue(cachedProp.propertyName, currentVal);
            
            // Также принудительно сообщаем компоненту сбросить оверрайд свойства, если это необходимо,
            // но в нашей текущей архитектуре достаточно очистить флаг сцены:
            prefabRoot->markInterfaceOverrideDirty(cachedProp.displayName, false);
        }
    }
     */
    
    std::cout << "[Engine Editor] Интерфейс и дефолты префаба успешно сохранены: "
    << prefabRoot->prefabAssetPath << std::endl;
}
