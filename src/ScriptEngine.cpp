//
//  ScriptEngine.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 03.09.26.
//

#include "ScriptEngine.h"
#include "ScriptComponent.h"
#include "Entity.h"
#include "Input.h"

#include "World.h"

#include <glm/glm.hpp>

#include <scriptstdstring/scriptstdstring.h>
#include <scriptbuilder/scriptbuilder.h>
#include <scripthandle/scripthandle.h>

#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>

static CScriptHandle GetScriptOnEntity(Entity* entity, const std::string& className);
static void GenerateScriptPredefined(const asIScriptEngine* engine, const std::string& path);

static CScriptBuilder g_builder;

//ScriptEngine::~ScriptEngine()
//{
//    if (m_Context) m_Context->Release();
//    if (m_asEngine) m_asEngine->ShutDownAndRelease();
//}

bool ScriptEngine::init()
{
    // 1. Создаем движок AngelScript
    m_asEngine = asCreateScriptEngine();
    if (!m_asEngine) return false;
    
    // Настраиваем вывод ошибок
    m_asEngine->SetMessageCallback(asFUNCTION(MessageCallback), this, asCALL_CDECL);
    
    // 2. Регистрируем стандартные строки из аддонов
    RegisterStdString(m_asEngine);
    RegisterScriptHandle(m_asEngine);
    
    // 3. Создаем один контекст для выполнения (переиспользуемый)
    m_Context = m_asEngine->CreateContext();
    
    // 4. Регистрируем API нашего движка (математику, GameObject и т.д.)
    RegisterEngineAPI();
    GenerateScriptPredefined(m_asEngine, "assets/scripts/as.predefined");
    
    std::cout << "Success to init AngelScript!" << std::endl;
    
#ifdef AS_PROCESS_METADATA
    std::cout << "AngelScript uses metadata" << std::endl;
#endif
    
    return true;
}

void ScriptEngine::MessageCallback(const asSMessageInfo* msg, void* param)
{
    // Восстанавливаем указатель на наш движок скриптов из void*
    ScriptEngine* scriptEngine = static_cast<ScriptEngine*>(param);
    if (!scriptEngine) return;
    
    ScriptLogMessage logMsg;
    logMsg.section = msg->section;
    logMsg.row = msg->row;
    logMsg.col = msg->col;
    logMsg.text = msg->message;
    
    // Определяем тип сообщения
    logMsg.type = ScriptLogType::Info;
    if (msg->type == asMSGTYPE_WARNING) logMsg.type = ScriptLogType::Warning;
    if (msg->type == asMSGTYPE_ERROR)   logMsg.type = ScriptLogType::Error;
    
    if (msg->type == asMSGTYPE_ERROR) {
        scriptEngine->AddLogMessage(logMsg);
    }
    
//    {
//        std::string type = "INFO";
//        if (msg->type == asMSGTYPE_WARNING) type = "WARNING";
//        if (msg->type == asMSGTYPE_ERROR)   type = "ERROR";
//        
//        std::cout << msg->section << " (" << msg->row << ", " << msg->col << ") : "
//        << type << " : " << msg->message << std::endl;
//    }
}

void RegisterMath(asIScriptEngine* engine);
void RegisterTransformComponent(asIScriptEngine* engine);
void RegisterAudioComponent(asIScriptEngine* engine);
void RegisterEntity(asIScriptEngine* engine);

void RegisterInputSystem(asIScriptEngine* engine);

void ScriptPrintCallback(const std::string& text)
{
    ScriptLogMessage msg;
    msg.type = ScriptLogType::ScriptPrint;
    msg.text = text;
    
    ScriptEngine::get().AddLogMessage(msg);
}

// Сюда мы будем добавлять функции движка, доступные в скриптах
void ScriptEngine::RegisterEngineAPI()
{
    int r = m_asEngine->RegisterGlobalFunction("void print(const string &in)", asFUNCTION(ScriptPrintCallback), asCALL_CDECL);
    assert(r >= 0);
    
    RegisterMath(m_asEngine);
    RegisterTransformComponent(m_asEngine);
    RegisterAudioComponent(m_asEngine);
    
    RegisterEntity(m_asEngine);
    m_asEngine->RegisterGlobalFunction("ref@ _GetScript(Entity@, const string &in)", asFUNCTION(GetScriptOnEntity), asCALL_CDECL);
    
    RegisterInputSystem(m_asEngine);
}

bool ScriptEngine::CompileAllScripts(const std::string& scriptsFolder)
{
    int r = g_builder.StartNewModule(m_asEngine, "Game");
    if (r < 0) return false;
    
    const char* monoBehaviourCode =
    "abstract class MonoBehaviour {"
    "    Entity@ GetOwner() { return m_owner; }"
    
    "    TransformComponent@ GetTransform() { return m_owner.GetTransform(); }"
    
    "    ref@ GetScript(const string &in className) {"
    "        return _GetScript(m_owner, className);"
    "    }"
    
    "    void start() { }"
    "    void update(float dt) { }"
    
    "    private void _Init(Entity@ owner) {"
    "        @m_owner = owner;"
    "    }"
    
    "    private Entity@ m_owner;"
    
    "}";
    
    g_builder.AddSectionFromMemory("common", monoBehaviourCode);
    if (r < 0) return false;
    
    for (const auto& entry : std::filesystem::recursive_directory_iterator(scriptsFolder))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".as")
        {
            std::string sectionName = entry.path().string();
            
            std::ifstream file(entry.path());
            if (!file.is_open()) {
                std::cout << "Failed to open file: " << sectionName << std::endl;
                continue;
            }
            
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string scriptCode = buffer.str();
            
            g_builder.AddSectionFromMemory(sectionName.c_str(), scriptCode.c_str());
            
            if (r < 0) {
                std::cout << "Failed to add " << sectionName << std::endl;
                return false;
            }
        }
    }
    
    r = g_builder.BuildModule();
    if (r < 0) {
        std::cout << "Compilation failed for game scripts! code:" << r << std::endl;
        return false;
    }
    
    std::cout << "All scripts compiled!" << std::endl;
    return true;
}

void ScriptEngine::HotReloadAllScripts(const std::string& scriptsFolder)
{
    ClearLogs();
    
    std::cout << "Starting Hot Reload..." << std::endl;
    
    // 1. ШАГ: Сохраняем состояние ВСЕХ скриптов на сцене
    for (auto& entity : g_currentScene->m_entities)
    {
        std::vector<ScriptComponent*> scripts;
        entity->getComponents(scripts);
        
        for (auto& script : scripts)
        {
            script->saveStateSnapshot();
        }
    }
    
    // 2. ШАГ: Перекомпилируем общий игровой модуль
    // Билдер сотрет старый модуль, создаст новые типы данных и обновит метаданные
    bool compileSuccess = CompileAllScripts(scriptsFolder);
    
    if (!compileSuccess) {
        std::cout << "Hot Reload failed due to compilation errors!" << std::endl;
        return;
    }
    
    // 3. ШАГ: Восстанавливаем объекты скриптов с сохраненными данными
    for (auto& entity : g_currentScene->m_entities)
    {
        std::vector<ScriptComponent*> scripts;
        entity->getComponents(scripts);
        
        for (auto& script : scripts)
        {
            script->recreateScriptObject();
            script->loadStateSnapshot();
        }
    }
    
    std::cout << "Hot Reload successfully completed!" << std::endl;
}

std::vector<std::string> ScriptEngine::GetMetadataForTypeProperty(int typeId, int varIdx)
{
    return g_builder.GetMetadataForTypeProperty(typeId, varIdx);
}

void RegisterMath(asIScriptEngine* engine)
{
    int r;
    
    // 1. Регистрируем тип Vector3 (сопоставляем с glm::vec3)
    r = engine->RegisterObjectType("Vector3", sizeof(glm::vec3), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_C); assert(r >= 0);
    
    // 2. Регистрируем свойства (поля x, y, z)
    r = engine->RegisterObjectProperty("Vector3", "float x", asOFFSET(glm::vec3, x)); assert(r >= 0);
    r = engine->RegisterObjectProperty("Vector3", "float y", asOFFSET(glm::vec3, y)); assert(r >= 0);
    r = engine->RegisterObjectProperty("Vector3", "float z", asOFFSET(glm::vec3, z)); assert(r >= 0);
    
    // 3. Регистрируем конструкторы
    // Стандартный конструктор Vector3()
    r = engine->RegisterObjectBehaviour("Vector3", asBEHAVE_CONSTRUCT, "void f()",
                                        asFUNCTION(+[](void* memory) { new(memory) glm::vec3(0.0f); }), asCALL_CDECL_OBJLAST); assert(r >= 0);
    
    // Конструктор с параметрами Vector3(x, y, z)
    r = engine->RegisterObjectBehaviour("Vector3", asBEHAVE_CONSTRUCT, "void f(float, float, float)",
                                        asFUNCTION(+[](float x, float y, float z, void* memory) { new(memory) glm::vec3(x, y, z); }), asCALL_CDECL_OBJLAST); assert(r >= 0);
}

void RegisterTransformComponent(asIScriptEngine* engine)
{
    int r;
    
    // Регистрируем TransformComponent. asOBJ_REF | asOBJ_NOCOUNT означает,
    // что AngelScript не управляет временем жизни этого C++ объекта (вы им управляете сами)
    r = engine->RegisterObjectType("TransformComponent", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    
    // Регистрируем свойства как методы-геттеры/сеттеры или напрямую, если тип зарегистрирован.
    // Так как Vector3 зарегистрирован как Value-тип, можно дать прямой доступ к полям:
    r = engine->RegisterObjectProperty("TransformComponent", "Vector3 position", asOFFSET(TransformComponent, position)); assert(r >= 0);
    r = engine->RegisterObjectProperty("TransformComponent", "Vector3 rotation", asOFFSET(TransformComponent, rotation)); assert(r >= 0);
    r = engine->RegisterObjectProperty("TransformComponent", "Vector3 scale",    asOFFSET(TransformComponent, scale));    assert(r >= 0);
}

void RegisterAudioComponent(asIScriptEngine* engine)
{
    // Регистрируем Entity как тип-ссылку (без подсчета ссылок)
    engine->RegisterObjectType("AudioComponent", 0, asOBJ_REF | asOBJ_NOCOUNT);
    
    // Регистрируем метод получения TransformComponent у Entity
    engine->RegisterObjectMethod("AudioComponent", "void Play()",
                                 asMETHOD(AudioComponent, play), asCALL_THISCALL);
}

void RegisterEntity(asIScriptEngine* engine)
{
    // Регистрируем Entity как тип-ссылку (без подсчета ссылок)
    engine->RegisterObjectType("Entity", 0, asOBJ_REF | asOBJ_NOCOUNT);
    
    // Регистрируем метод получения TransformComponent у Entity
    engine->RegisterObjectMethod("Entity", "TransformComponent@ GetTransform()",
                                 asMETHOD(Entity, getComponent<TransformComponent>), asCALL_THISCALL);
    
    engine->RegisterObjectMethod("Entity", "AudioComponent@ GetAudio()",
                                 asMETHOD(Entity, getComponent<AudioComponent>), asCALL_THISCALL);
    
    engine->RegisterObjectMethod("Entity", "string GetName() const",
                                 asMETHOD(Entity, GetName), asCALL_THISCALL);
}

void RegisterInputEnums(asIScriptEngine* engine)
{
    int r;
    // Регистрируем перечисление для клавиш
    r = engine->RegisterEnum("KeyCode"); assert(r >= 0);
    
    // Привязываем ваши коды клавиш движка (здесь пример для GLFW, замените на свои)
    r = engine->RegisterEnumValue("KeyCode", "Space",    32);  assert(r >= 0);
    r = engine->RegisterEnumValue("KeyCode", "A",        65);  assert(r >= 0);
    r = engine->RegisterEnumValue("KeyCode", "D",        68);  assert(r >= 0);
    r = engine->RegisterEnumValue("KeyCode", "W",        87);  assert(r >= 0);
    r = engine->RegisterEnumValue("KeyCode", "S",        83);  assert(r >= 0);
    r = engine->RegisterEnumValue("KeyCode", "Escape",   256); assert(r >= 0);
    
    // Аналогично для мыши
    r = engine->RegisterEnum("MouseButton"); assert(r >= 0);
    r = engine->RegisterEnumValue("MouseButton", "Left",   0); assert(r >= 0);
    r = engine->RegisterEnumValue("MouseButton", "Right",  1); assert(r >= 0);
}

void RegisterInputSystem(asIScriptEngine* engine)
{
    int r;
    
    // Сначала регистрируем enums
    RegisterInputEnums(engine);
    
    // Открываем пространство имен "Input"
    r = engine->SetDefaultNamespace("Input"); assert(r >= 0);
    
    // Регистрируем глобальные функции внутри этого пространства имен
    r = engine->RegisterGlobalFunction("bool GetKey(KeyCode key)", asFUNCTION(Input::isKeyPressed), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("bool GetKeyDown(KeyCode key)", asFUNCTION(Input::isKeyClicked), asCALL_CDECL); assert(r >= 0);
    
    // Регистрация позиции мыши (возвращает Vector3 по значению)
//    r = engine->RegisterGlobalFunction("Vector3 GetMousePosition()", asFUNCTION(EngineInput::GetMousePosition), asCALL_CDECL); assert(r >= 0);
    
    // Сбрасываем пространство имен обратно в глобальное, чтобы не сломать регистрацию других систем
    r = engine->SetDefaultNamespace(""); assert(r >= 0);
}

static CScriptHandle GetScriptOnEntity(Entity* entity, const std::string& className)
{
    CScriptHandle handle;
    if (!entity) return handle;
    
    // Проходим по всем C++ компонентам этой сущности
    for (auto scriptComp : entity->getComponents<ScriptComponent>())
    {
        // Проверяем имя класса внутри AngelScript объекта
        if (scriptComp->m_ScriptObject)
        {
            asITypeInfo* type = scriptComp->m_ScriptObject->GetObjectType();
            
            if (type && std::string(type->GetName()) == className)
            {
                handle.Set(scriptComp->m_ScriptObject, type);
                break;
            }
        }
    }
    
    return handle;
}


#include <format>
#include <fstream>
#include <string_view>

namespace
{
    template <class Stream>
    void printEnumList(const asIScriptEngine* engine, Stream& stream)
    {
        for (int i = 0; i < engine->GetEnumCount(); i++)
        {
            const auto e = engine->GetEnumByIndex(i);
            if (not e) continue;
            const std::string_view ns = e->GetNamespace();
            if (not ns.empty()) stream << std::format("namespace {} {{\n", ns);
            stream << std::format("enum {} {{\n", e->GetName());
            for (int j = 0; j < e->GetEnumValueCount(); ++j)
            {
                stream << std::format("\t{}", e->GetEnumValueByIndex(j, nullptr));
                if (j < e->GetEnumValueCount() - 1) stream << ",";
                stream << "\n";
            }
            stream << "}\n\n";
            if (not ns.empty()) stream << "}\n";
        }
    }
    
    template <class Stream>
    void printClassTypeList(const asIScriptEngine* engine, Stream& stream)
    {
        for (int i = 0; i < engine->GetObjectTypeCount(); i++)
        {
            const auto t = engine->GetObjectTypeByIndex(i);
            if (not t) continue;
            
            const std::string_view ns = t->GetNamespace();
            if (not ns.empty()) stream << std::format("namespace {} {{\n", ns);
            
            stream << std::format("class {}", t->GetName());
            if (t->GetSubTypeCount() > 0)
            {
                stream << "<";
                for (int sub = 0; sub < t->GetSubTypeCount(); ++sub)
                {
                    if (sub < t->GetSubTypeCount() - 1) stream << ", ";
                    const auto st = t->GetSubType(sub);
                    stream << st->GetName();
                }
                
                stream << ">";
            }
            
            stream << " {\n";
            for (int j = 0; j < t->GetBehaviourCount(); ++j)
            {
                asEBehaviours behaviours;
                const auto f = t->GetBehaviourByIndex(j, &behaviours);
                if (behaviours == asBEHAVE_CONSTRUCT
                    || behaviours == asBEHAVE_DESTRUCT)
                {
                    stream << std::format("\t{};\n", f->GetDeclaration(false, true, true));
                }
            }
            for (int j = 0; j < t->GetMethodCount(); ++j)
            {
                const auto m = t->GetMethodByIndex(j);
                stream << std::format("\t{};\n", m->GetDeclaration(false, true, true));
            }
            for (int j = 0; j < t->GetPropertyCount(); ++j)
            {
                stream << std::format("\t{};\n", t->GetPropertyDeclaration(j, true));
            }
            for (int j = 0; j < t->GetChildFuncdefCount(); ++j)
            {
                stream << std::format("\tfuncdef {};\n",
                                      t->GetChildFuncdef(j)->GetFuncdefSignature()->GetDeclaration(false));
            }
            stream << "}\n\n";
            if (not ns.empty()) stream << "}\n";
        }
    }
    
    template <class Stream>
    void printGlobalFunctionList(const asIScriptEngine* engine, Stream& stream)
    {
        for (int i = 0; i < engine->GetGlobalFunctionCount(); i++)
        {
            const auto f = engine->GetGlobalFunctionByIndex(i);
            if (not f) continue;
            const std::string_view ns = f->GetNamespace();
            if (not ns.empty()) stream << std::format("namespace {} {{ ", ns);
            stream << std::format("{};", f->GetDeclaration(false, false, true));
            if (not ns.empty()) stream << " }";
            stream << "\n\n";
        }
    }
    
    template <class Stream>
    void printGlobalPropertyList(const asIScriptEngine* engine, Stream& stream)
    {
        for (int i = 0; i < engine->GetGlobalPropertyCount(); i++)
        {
            const char* name;
            const char* ns0;
            int type;
            engine->GetGlobalPropertyByIndex(i, &name, &ns0, &type, nullptr, nullptr, nullptr, nullptr);
            
            const std::string t = engine->GetTypeDeclaration(type, true);
            if (t.empty()) continue;
            
            std::string_view ns = ns0;
            if (not ns.empty()) stream << std::format("namespace {} {{ ", ns);
            
            stream << std::format("{} {};", t, name);
            if (not ns.empty()) stream << " }";
            stream << "\n\n";
        }
    }
    
    template <class Stream>
    void printGlobalTypedef(const asIScriptEngine* engine, Stream& stream)
    {
//        for (int i = 0; i < engine->GetTypedefCount(); ++i)
//        {
//            const auto type = engine->GetTypedefByIndex(i);
//            if (not type) continue;
//            const std::string_view ns = type->GetNamespace();
//            if (not ns.empty()) stream << std::format("namespace {} {{\n", ns);
//            stream << std::format(
//                                  "typedef {} {};\n", engine->GetTypeDeclaration(type->GetTypedefTypeId()), type->GetName());
//            if (not ns.empty()) stream << "}\n";
//        }
    }
    
    template <class Stream>
    void printBaseClass(const asIScriptEngine* engine, Stream& stream)
    {
        stream << "abstract class MonoBehaviour {\n";
        stream << "    Entity@ GetOwner();\n";
        stream << "    TransformComponent@ GetTransform();\n";
        stream << "    ref@ GetScript(const string &in className);\n";
        stream << "    void start();\n";
        stream << "    void update(float dt);\n";
        stream << "}\n\n";
    }
}

/// @brief Generate 'as.predefined' file, which contains all defined symbols in C++. It is used by the language server.
static void GenerateScriptPredefined(const asIScriptEngine* engine, const std::string& path)
{
    assert(path.ends_with("as.predefined"));
    
    std::ofstream stream{path};
    
    stream << "// ===================================================\n";
    stream << "// АВТОГЕНЕРИРУЕМОЕ API ДВИЖКА ДЛЯ АВТОДОПОЛНЕНИЯ\n";
    stream << "// НЕ РЕДАКТИРУЙТЕ ЭТОТ ФАЙЛ ВРУЧНУЮ\n";
    stream << "// ===================================================\n\n";
    
    printEnumList(engine, stream);
    
    printClassTypeList(engine, stream);
    
    printGlobalFunctionList(engine, stream);
    
    printGlobalPropertyList(engine, stream);
    
    printGlobalTypedef(engine, stream);
    
    printBaseClass(engine, stream);
}
