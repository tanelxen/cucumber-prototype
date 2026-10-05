//
//  ScriptComponent.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 02.09.26.
//

#include "ScriptComponent.h"
#include "ScriptEngine.h"

#include "TransformComponent.h"
#include "AnimatorComponent.h"
#include "World.h"

#include <imgui.h>
#include <iostream>

static asIScriptObject* GetScriptObjectOnEntity(Entity* entity, const std::string& className);

ScriptComponent::ScriptComponent() : Component(Flags::RequiresUpdate | Flags::RequiresFixedUpdate)
{
    
}

void ScriptComponent::init(const std::string& className)
{
    this->m_ClassName = className;
//    this->scriptPath = scriptPath;

    asIScriptEngine* asEngine = ScriptEngine::get().GetRawEngine();
    
    // ВСЕГДА берем один и тот же модуль "Game"
    asIScriptModule* mod = asEngine->GetModule("Game", asGM_ONLY_IF_EXISTS);
    if (!mod) return;
    
    // Находим тип класса в скрипте
    asITypeInfo* type = mod->GetTypeInfoByName(m_ClassName.c_str());
    if (!type) {
        std::cout << "Class " << m_ClassName << " not found in module Game" << std::endl;
        return;
    }
    
    // Находим фабричный метод (конструктор) класса: "ClassName@ ClassName()"
    std::string factoryDecl = m_ClassName + "@ " + m_ClassName + "()";
    asIScriptFunction* factory = type->GetFactoryByDecl(factoryDecl.c_str());
    
    if (factory)
    {
        asIScriptContext* ctx = ScriptEngine::get().GetContext();
        ctx->Prepare(factory);
        
        if (ctx->Execute() == asEXECUTION_FINISHED) {
            // Получаем созданный объект скрипта и увеличиваем счетчик ссылок, чтобы он не удалился
            m_ScriptObject = *static_cast<asIScriptObject**>(ctx->GetAddressOfReturnValue());
            m_ScriptObject->AddRef();
        }
        
        const char* sectionName = nullptr;
        factory->GetDeclaredAt(&sectionName, NULL, NULL);
        scriptPath = sectionName;
    }
    
    // 2. АВТОМАТИЧЕСКАЯ ИНИЦИАЛИЗАЦИЯ ОСНОВЫ (Наш MonoBehaviour)
    if (m_ScriptObject)
    {
        // Ищем метод _Init, который перешел по наследству от MonoBehaviour
        asIScriptFunction* initMethod = type->GetMethodByDecl("void _Init(Entity@)");
        
        if (initMethod)
        {
            asIScriptContext* ctx = ScriptEngine::get().GetContext();
            ctx->Prepare(initMethod);
            ctx->SetObject(m_ScriptObject);
            ctx->SetArgAddress(0, this->owner); // Безопасно передаем C++ указатель на Entity
            ctx->Execute();
        }
    }
    
    // Кэшируем указатели на методы Start и Update для быстродействия
    m_StartMethod = type->GetMethodByDecl("void start()");
    m_UpdateMethod = type->GetMethodByDecl("void update(float)");
    
    
    m_isInitialized = true;
}

void ScriptComponent::awake()
{
    // Все сущности и компоненты на сцене созданы - восстанавливаем связи
    loadStateSnapshot();
}

void ScriptComponent::start()
{
    if (!m_ScriptObject || !m_StartMethod) return;
    
    asIScriptContext* ctx = ScriptEngine::get().GetContext();
    ctx->Prepare(m_StartMethod);
    ctx->SetObject(m_ScriptObject);
    ctx->Execute();
}

void ScriptComponent::update(float dt)
{
    if (!m_ScriptObject || !m_UpdateMethod || !m_isPlaying) return;
    
    asIScriptContext* ctx = ScriptEngine::get().GetContext();
    ctx->Prepare(m_UpdateMethod);
    ctx->SetObject(m_ScriptObject);
    ctx->SetArgFloat(0, dt);
    ctx->Execute();
}

ScriptComponent::~ScriptComponent()
{
    if (m_ScriptObject) {
        m_ScriptObject->Release();
    }
}

void ScriptComponent::recreateScriptObject()
{
    init(m_ClassName);
}

void ScriptComponent::drawProperties()
{
    if (!m_ScriptObject) return;
    
    asITypeInfo* type = m_ScriptObject->GetObjectType();
    
    if (ImGui::TreeNode("Variables"))
    {
        int typeId = type->GetTypeId();
        
        for (asUINT i = 0; i < type->GetPropertyCount(); i++)
        {
            const char* propName;
            int propTypeId;
            bool isPrivate;
            type->GetProperty(i, &propName, &propTypeId, &isPrivate);
            
            if (isPrivate) continue; // Не показываем приватные поля
            
            std::vector<std::string> metadataList = ScriptEngine::get().GetMetadataForTypeProperty(typeId, i);
            
            bool hideInInspector = false;
            std::string headerText = "";
            std::string tooltipText = "";
            bool hasRange = false;
            float minVal = 0.0f, maxVal = 1.0f;
            
            
            void* varPtr = m_ScriptObject->GetAddressOfProperty(i);
            
            for (const auto& metadata : metadataList)
            {
                size_t headerPos = metadata.find("Header(");
                size_t tooltipPos = metadata.find("Tooltip(");
                
                if (metadata == "HideInInspector")
                {
                    hideInInspector = true;
                }
                
                if (headerPos != std::string::npos)
                {
                    size_t start = metadata.find("\"", headerPos) + 1;
                    size_t end = metadata.find("\"", start);
                    headerText = metadata.substr(start, end - start);
                }
                
                if (tooltipPos != std::string::npos)
                {
                    size_t start = metadata.find("(", tooltipPos) + 1;
                    size_t end = metadata.find(")", start);
                    tooltipText = metadata.substr(start, end - start);
                }
                
                if (metadata.find("Range(") == 0)
                {
                    hasRange = true;
                    sscanf(metadata.c_str(), "Range(%f, %f)", &minVal, &maxVal);
                }
            }
            
            if (hideInInspector) continue;
            
            if (!headerText.empty()) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "%s", headerText.c_str());
                ImGui::Separator();
            }
            
            if (propTypeId == asTYPEID_FLOAT)
            {
                if (hasRange) {
                    ImGui::SliderFloat(propName, static_cast<float*>(varPtr), minVal, maxVal);
                } else {
                    ImGui::DragFloat(propName, static_cast<float*>(varPtr), 0.05f);
                }
            }
            else if (propTypeId == asTYPEID_INT32)
            {
                if (hasRange) {
                    ImGui::SliderInt(propName, static_cast<int*>(varPtr), static_cast<int>(minVal), static_cast<int>(maxVal));
                } else {
                    ImGui::DragInt(propName, static_cast<int*>(varPtr));
                }
            }
            else if (propTypeId == asTYPEID_BOOL)
            {
                ImGui::Checkbox(propName, static_cast<bool*>(varPtr));
            }
            // Для строк (если зарегистрирован шаблон std::string)
            else if (propTypeId == ScriptEngine::get().GetRawEngine()->GetTypeIdByDecl("string"))
            {
                std::string* str = (std::string*)varPtr;
                char buf[256];
                strncpy(buf, str->c_str(), sizeof(buf));
                if (ImGui::InputText(propName, buf, sizeof(buf))) {
                    *str = buf;
                }
            }
            else if (propTypeId & asTYPEID_OBJHANDLE)
            {
                // Получаем тип, на который указывает ссылка (например, TransformComponent или Player)
                asITypeInfo* propTypeInfo = ScriptEngine::get().GetRawEngine()->GetTypeInfoById(propTypeId);
                std::string typeName = propTypeInfo ? propTypeInfo->GetName() : "Unknown";
                
                // Получаем текущее значение указателя, сохраненное в скрипте
                void** ptrToHandle = static_cast<void**>(m_ScriptObject->GetAddressOfProperty(i));
                void* currentActualPtr = *ptrToHandle;
                
                std::string buttonText = "None (" + typeName + ")";
                
                if (currentActualPtr != nullptr)
                {
                    // Если это C++ компонент, мы можем вытащить имя его Entity
                    // Если это скрипт, через UserData достаем ScriptComponent и его Entity
                    
                    if (typeName == "TransformComponent")
                    {
                        TransformComponent* comp = static_cast<TransformComponent*>(currentActualPtr);
                        
                        if (comp && comp->owner) {
                            buttonText = comp->owner->m_name + "." + typeName;
                        }
                    }
                    if (typeName == "AnimatorComponent")
                    {
                        AnimatorComponent* comp = static_cast<AnimatorComponent*>(currentActualPtr);
                        
                        if (comp && comp->owner) {
                            buttonText = comp->owner->m_name + "." + typeName;
                        }
                    }
                    else if (typeName == "Entity")
                    {
                        if (Entity* entity = static_cast<Entity*>(currentActualPtr))
                        {
                            buttonText = entity->m_name;
                        }
                    }
                    else
                    {
                        buttonText = "Linked " + typeName;
                    }
                }
                
                ImGui::Text("%s", propName);
                ImGui::SameLine();
                
                // Рисуем кнопку-ссылку
                ImGui::PushStyleColor(ImGuiCol_Button, currentActualPtr ? ImVec4(0.2f, 0.4f, 0.2f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                ImGui::Button(buttonText.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0));
                ImGui::PopStyleColor();
                
                // --- ПРИЕМ DRAG-N-DROP ИЗ ИЕРАРХИИ ---
                if (ImGui::BeginDragDropTarget())
                {
                    // Предположим, из окна Hierarchy вы перетаскиваете указатель на Entity* с типом payload "HIERARCHY_ENTITY"
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY"))
                    {
                        Entity* draggedEntity = *static_cast<Entity**>(payload->Data);
                        
                        if (draggedEntity != nullptr)
                        {
                            // ИНФРАСТРУКТУРА СВЯЗЫВАНИЯ:
                            // Ситуация А: Свойство в скрипте требует нативный C++ компонент (например, TransformComponent)
                            if (typeName == "TransformComponent")
                            {
                                TransformComponent* comp = draggedEntity->getComponent<TransformComponent>();
                                if (comp) {
                                    // Записываем C++ указатель напрямую в память AngelScript
                                    // ВАЖНО: так как тип зарегистрирован как asOBJ_NOCOUNT, мы просто копируем адрес
                                    *ptrToHandle = comp;
                                }
                            }
                            if (typeName == "AnimatorComponent")
                            {
                                AnimatorComponent* comp = draggedEntity->getComponent<AnimatorComponent>();
                                if (comp) {
                                    // Записываем C++ указатель напрямую в память AngelScript
                                    // ВАЖНО: так как тип зарегистрирован как asOBJ_NOCOUNT, мы просто копируем адрес
                                    *ptrToHandle = comp;
                                }
                            }
                            else if (typeName == "Entity")
                            {
                                *ptrToHandle = draggedEntity;
                            }
                            // Ситуация Б: Свойство требует кастомный скрипт (например, Player)
                            else
                            {
                                // Ищем ScriptComponent на перетащенной сущности, внутри которого живет класс с именем typeName
                                asIScriptObject* targetScriptObj = GetScriptObjectOnEntity(draggedEntity, typeName);
                                if (targetScriptObj) {
                                    // Перед тем как записать новый скриптовый объект, нужно правильно управлять подсчетом ссылок!
                                    if (currentActualPtr) ((asIScriptObject*)currentActualPtr)->Release();
                                    
                                    targetScriptObj->AddRef(); // Увеличиваем счетчик, так как теперь этот скрипт им владеет
                                    *ptrToHandle = targetScriptObj;
                                }
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
                
                // Контекстное меню по правому клику для сброса ссылки в None
                if (ImGui::BeginPopupContextItem())
                {
                    if (ImGui::MenuItem("Clear Reference"))
                    {
                        if (currentActualPtr && !(propTypeId & asTYPEID_OBJHANDLE)) {
                            // Если это был скриптовый объект, уменьшаем счетчик ссылок
                            // Нативные C++ типы (no count) просто зануляем
                            asITypeInfo* info = ScriptEngine::get().GetRawEngine()->GetTypeInfoById(propTypeId);
                            if (info && (info->GetFlags() & asOBJ_SCRIPT_OBJECT)) {
                                ((asIScriptObject*)currentActualPtr)->Release();
                            }
                        }
                        *ptrToHandle = nullptr; // Сбрасываем в null
                    }
                    ImGui::EndPopup();
                }
            }

            
            // Рисуем подсказку при наведении
            if (!tooltipText.empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", tooltipText.c_str());
            }
        }
        
        ImGui::TreePop();
    }
}

void ScriptComponent::getStateSnapshot(nlohmann::json& snapshot) const
{
    if (!m_ScriptObject) return snapshot;
    
    asITypeInfo* type = m_ScriptObject->GetObjectType();
    
    for (asUINT i = 0; i < type->GetPropertyCount(); i++)
    {
        const char* propName;
        int propTypeId;
        bool isPrivate;
        type->GetProperty(i, &propName, &propTypeId, &isPrivate);
        
        if (isPrivate) continue; // Пропускаем приватные поля
        
        void* varPtr = m_ScriptObject->GetAddressOfProperty(i);
        
        if (propTypeId == asTYPEID_FLOAT)
        {
            snapshot[propName] = *static_cast<float*>(varPtr);
        }
        else if (propTypeId == asTYPEID_INT32)
        {
            snapshot[propName] = *static_cast<int*>(varPtr);
        }
        else if (propTypeId == asTYPEID_BOOL)
        {
            snapshot[propName] = *static_cast<bool*>(varPtr);
        }
        else if (propTypeId == ScriptEngine::get().GetRawEngine()->GetTypeIdByDecl("string"))
        {
            snapshot[propName] = *static_cast<std::string*>(varPtr);
        }
        else if (propTypeId & asTYPEID_OBJHANDLE)
        {
            // Получаем тип, на который указывает ссылка (например, TransformComponent или Player)
            asITypeInfo* propTypeInfo = ScriptEngine::get().GetRawEngine()->GetTypeInfoById(propTypeId);
            if (propTypeInfo == nullptr) continue;
            
            std::string typeName = propTypeInfo->GetName();
            
            // Получаем текущее значение указателя, сохраненное в скрипте
            void** ptrToHandle = static_cast<void**>(m_ScriptObject->GetAddressOfProperty(i));
            void* currentActualPtr = *ptrToHandle;
            
            if (currentActualPtr == nullptr) continue;
            
            if (typeName == "Entity")
            {
                Entity* entity = static_cast<Entity*>(currentActualPtr);
                
                if (entity)
                {
                    snapshot[propName]["type"] = typeName;
                    
                    if (owner->isPrefabInstance() && !owner->isPrefabRoot)
                    {
                        snapshot[propName]["path"] = entity->getPathRelativeTo(owner);
                    }
                    else
                    {
                        snapshot[propName]["uid"] = entity->uid;
                    }
                }
            }
            
            if (typeName == "TransformComponent")
            {
                TransformComponent* comp = static_cast<TransformComponent*>(currentActualPtr);
                
                if (comp && comp->owner)
                {
                    snapshot[propName]["type"] = typeName;
                    
                    if (owner->isPrefabInstance() && !owner->isPrefabRoot)
                    {
                        snapshot[propName]["path"] = comp->owner->getPathRelativeTo(owner);
                    }
                    else
                    {
                        snapshot[propName]["owner"] = comp->owner->uid;
                    }
                }
            }
            
            if (typeName == "AnimatorComponent")
            {
                AnimatorComponent* comp = static_cast<AnimatorComponent*>(currentActualPtr);
                
                if (comp && comp->owner)
                {
                    snapshot[propName]["type"] = typeName;
                    
                    if (owner->isPrefabInstance() && !owner->isPrefabRoot)
                    {
                        snapshot[propName]["path"] = comp->owner->getPathRelativeTo(owner);
                    }
                    else
                    {
                        snapshot[propName]["owner"] = comp->owner->uid;
                    }
                }
            }
        }
    }
}

void ScriptComponent::saveStateSnapshot()
{
    m_StateSnapshot = nlohmann::json::object();
    getStateSnapshot(m_StateSnapshot);
    
    m_ScriptObject->Release();
}

void ScriptComponent::loadStateSnapshot()
{
    for (auto& snapshot : m_StateSnapshot)
    {
        // Первый снепшот это значения из префаба, второй - оверрайды инстанца на сцене
        loadScriptStateFromValues(snapshot);
    }
    
    std::cout << std::endl;
    
    // HACK: нужно чтобы awake смог вызваться повторно после загрузки сцены
    // поэтому не удаляем снэпшот
    //m_StateSnapshot.clear();
}

void ScriptComponent::loadScriptStateFromValues(const nlohmann::json& snapshot)
{
    if (!m_ScriptObject || snapshot.empty()) return;
    
    asITypeInfo* type = m_ScriptObject->GetObjectType();
    asIScriptEngine* engine = m_ScriptObject->GetEngine();
    
    for (asUINT i = 0; i < type->GetPropertyCount(); i++)
    {
        const char* propName;
        int propTypeId;
        bool isPrivate;
        type->GetProperty(i, &propName, &propTypeId, &isPrivate);
        
        if (isPrivate || !snapshot.contains(propName)) continue;
        
        // Получаем адрес, где лежит свойство
        void* varPtr = m_ScriptObject->GetAddressOfProperty(i);
        
        if (propTypeId == asTYPEID_FLOAT)
        {
            *static_cast<float*>(varPtr) = snapshot[propName].get<float>();
        }
        else if (propTypeId == asTYPEID_INT32)
        {
            *static_cast<int*>(varPtr) = snapshot[propName].get<int>();
        }
        else if (propTypeId == asTYPEID_BOOL)
        {
            *static_cast<bool*>(varPtr) = snapshot[propName].get<bool>();
        }
        else if (propTypeId == engine->GetTypeIdByDecl("string"))
        {
            *static_cast<std::string*>(varPtr) = snapshot[propName].get<std::string>();
        }
        else if (propTypeId & asTYPEID_OBJHANDLE)
        {
            auto& propData = snapshot[propName];
            
            if (!propData.contains("type")) continue;
            std::string type = propData["type"].get<std::string>();
            
            if (type == "Entity")
            {
                Entity* entity = nullptr;
                
                if (propData.contains("path"))
                {
                    std::string path = propData["path"].get<std::string>();
                    entity = owner->findEntityByPath(path);
                }
                else if (propData.contains("uid"))
                {
                    UID ownerUID = propData["uid"].get<std::string>();
                    entity = owner->scene->findEntityByUID(ownerUID);
                }
                
                if (entity == nullptr) continue;
                
                void** ptrToHandle = static_cast<void**>(m_ScriptObject->GetAddressOfProperty(i));
                *ptrToHandle = entity;
            }
            else if (type == "TransformComponent")
            {
                Entity* entity = nullptr;
                
                if (propData.contains("path"))
                {
                    std::string path = propData["path"].get<std::string>();
                    entity = owner->findEntityByPath(path);
                }
                else if (propData.contains("owner"))
                {
                    UID ownerUID = propData["owner"].get<std::string>();
                    entity = owner->scene->findEntityByUID(ownerUID);
                }

                if (entity == nullptr) continue;
                
                TransformComponent* comp = entity->getComponent<TransformComponent>();
                if (comp == nullptr) continue;
                
                void** ptrToHandle = static_cast<void**>(m_ScriptObject->GetAddressOfProperty(i));
                *ptrToHandle = comp;
            }
            else if (type == "AnimatorComponent")
            {
                Entity* entity = nullptr;
                
                if (propData.contains("path"))
                {
                    std::string path = propData["path"].get<std::string>();
                    entity = owner->findEntityByPath(path);
                }
                else if (propData.contains("owner"))
                {
                    UID ownerUID = propData["owner"].get<std::string>();
                    entity = owner->scene->findEntityByUID(ownerUID);
                }
                
                if (entity == nullptr) continue;
                
                AnimatorComponent* comp = entity->getComponent<AnimatorComponent>();
                if (comp == nullptr) continue;
                
                void** ptrToHandle = static_cast<void**>(m_ScriptObject->GetAddressOfProperty(i));
                *ptrToHandle = comp;
            }
            
            /*
            // 1. Очищаем бит хэндла, чтобы GetTypeInfoById понял, о каком классе речь
            int actualTypeId = propTypeId & ~asTYPEID_OBJHANDLE;
            
            asITypeInfo* typeInfo = engine->GetTypeInfoById(actualTypeId);
            if (typeInfo)
            {
                asQWORD flags = typeInfo->GetFlags();
                
                // 2. Так как это хэндл (@), varPtr хранит указатель на указатель (void**).
                // Достаем сам фактический адрес объекта:
                void* actualObjPtr = *static_cast<void**>(varPtr);
                
                if (actualObjPtr == nullptr) {
                    // Хэндл не инициализирован (равен null в скрипте)
                    continue;
                }
                
                // 3. Проверяем флаги типа
                if (flags & asOBJ_SCRIPT_OBJECT)
                {
                    // Это внутренний скриптовый класс. Работать как с C++ классом нельзя!
                    asIScriptObject* scriptObj = static_cast<asIScriptObject*>(actualObjPtr);
                    // ... обработка скриптового объекта ...
                }
                else if (flags & asOBJ_APP_CLASS)
                {
                    // Это ваш зарегистрированный C++ класс.
                    // Теперь actualObjPtr можно безопасно приводить к вашему базовому классу C++
                    // MyBaseClass* myClass = static_cast<MyBaseClass*>(actualObjPtr);
                }
            }
             */
        }
        else if (propTypeId & asTYPEID_MASK_OBJECT)
        {
            // НА ЗАМЕТКУ: Сюда код попадет, если объект хранится ПО ЗНАЧЕНИЮ (например, "MyClass prop;")
            // В этом случае varPtr — это уже прямой указатель на объект, разыменовывать его не надо.
            asITypeInfo* typeInfo = engine->GetTypeInfoById(propTypeId);
            if (typeInfo)
            {
                asQWORD flags = typeInfo->GetFlags();
                if (flags & asOBJ_APP_CLASS) { /* ... */ }
            }
        }
    }
}

void ScriptComponent::save(nlohmann::json& j) const
{
    j["className"] = m_ClassName;
    
    auto snapshot = nlohmann::json::object();
    getStateSnapshot(snapshot);
    
    j["fields"] = snapshot;
}

void ScriptComponent::load(const nlohmann::json& j)
{
    m_ClassName = j.at("className").get<std::string>();
    
    if (j.contains("fields"))
    {
        m_StateSnapshot += j.at("fields");
    }
    
    if (!m_isInitialized)
    {
        init(m_ClassName);
    }
}

static asIScriptObject* GetScriptObjectOnEntity(Entity* entity, const std::string& className)
{
    if (!entity) return nullptr;
    
    asIScriptObject* object = nullptr;
    
    for (auto scriptComp : entity->getComponents<ScriptComponent>())
    {
        // Проверяем имя класса внутри AngelScript объекта
        if (scriptComp->m_ScriptObject)
        {
            asITypeInfo* type = scriptComp->m_ScriptObject->GetObjectType();
            
            if (type && std::string(type->GetName()) == className)
            {
                object = scriptComp->m_ScriptObject;
                break;
            }
        }
    }
    
    return object;
}
