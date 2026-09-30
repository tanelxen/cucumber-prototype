//
//  ScriptEngine.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 03.09.26.
//

#include "ScriptEngine.h"
#include "ScriptComponent.h"
#include "AudioComponent.h"
#include "AnimatorComponent.h"
#include "TransformComponent.h"
#include "MovementComponent.h"
#include "CollisionComponent.h"

#include "Entity.h"
#include "Input.h"

#include "SpriteRenderer.h"
#include "ParticleSystem.h"
#include "DecalManager.h"
#include "HUD.h"

#include "World.h"

#include <glm/glm.hpp>

#include <scriptstdstring/scriptstdstring.h>
#include <scriptbuilder/scriptbuilder.h>
#include <scripthandle/scripthandle.h>
#include <scripthelper/scripthelper.h>
#include <scriptarray/scriptarray.h>

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
    
    RegisterScriptArray(m_asEngine, true);
    
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
void RegisterAnimatorComponent(asIScriptEngine* engine);
void RegisterMovementComponent(asIScriptEngine* engine);
void RegisterCollisionComponent(asIScriptEngine* engine);
void RegisterEntity(asIScriptEngine* engine);

void RegisterInputSystem(asIScriptEngine* engine);
void RegisterSpriteRenderer(asIScriptEngine* engine);
void RegisterUI(asIScriptEngine* engine);
void RegisterPhysics(asIScriptEngine* engine);

void ScriptPrintCallback(const std::string& text)
{
    ScriptLogMessage msg;
    msg.type = ScriptLogType::ScriptPrint;
    msg.text = text;
    
    ScriptEngine::get().AddLogMessage(msg);
}

static Entity* FindEntityByName(const std::string& name)
{
    return g_currentScene->findEntityByName(name);
}

// Сюда мы будем добавлять функции движка, доступные в скриптах
void ScriptEngine::RegisterEngineAPI()
{
    int r = m_asEngine->RegisterGlobalFunction("void print(const string &in)", asFUNCTION(ScriptPrintCallback), asCALL_CDECL);
    assert(r >= 0);
    
    RegisterMath(m_asEngine);
    RegisterTransformComponent(m_asEngine);
    RegisterAudioComponent(m_asEngine);
    RegisterAnimatorComponent(m_asEngine);
    RegisterMovementComponent(m_asEngine);
    RegisterCollisionComponent(m_asEngine);
    
    RegisterEntity(m_asEngine);
    m_asEngine->RegisterGlobalFunction("ref@ _GetScript(Entity@ entity, const string &in name)", asFUNCTION(GetScriptOnEntity), asCALL_CDECL);
    m_asEngine->RegisterGlobalFunction("Entity@ FindEntityByName(const string &in name)", asFUNCTION(FindEntityByName), asCALL_CDECL);
    
    RegisterInputSystem(m_asEngine);
    RegisterSpriteRenderer(m_asEngine);
    RegisterUI(m_asEngine);
    RegisterPhysics(m_asEngine);
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

// Для asCALL_CDECL_OBJFIRST:
// Сначала идет ВЕКТОР (объект), затем аргумент из скрипта (float f)
glm::vec3 Vec3MulFloat_ObjFirst(const glm::vec3& v, float f)
{
    return v * f;
}

// Для asCALL_CDECL_OBJLAST:
// Сначала идет аргумент из скрипта (float f), и только ПОТОМ ВЕКТОР (объект)
glm::vec3 FloatMulVec3_ObjLast(float f, const glm::vec3& v)
{
    return f * v;
}

// Умножение на число с присваиванием: vec *= float
// Возвращаем ссылку на vec& для поддержки цепочек вызовов в C++,
// но AngelScript ожидает возвращаемое значение в соответствии с сигнатурой.
glm::vec3& Vec3MulAssignFloat(float f, glm::vec3& v)
{
    v *= f;
    return v;
}

// Операции между векторами (по значению)
glm::vec3 Vec3AddVec3(const glm::vec3& v1, const glm::vec3& v2) { return v1 + v2; }
glm::vec3 Vec3SubVec3(const glm::vec3& v1, const glm::vec3& v2) { return v1 - v2; }
glm::vec3 Vec3MulVec3(const glm::vec3& v1, const glm::vec3& v2) { return v1 * v2; } // Покомпонентное

// С присваиванием (межвекторные)
glm::vec3& Vec3AddAssignVec3(const glm::vec3& v2, glm::vec3& v1) { v1 += v2; return v1; }
glm::vec3& Vec3SubAssignVec3(const glm::vec3& v2, glm::vec3& v1) { v1 -= v2; return v1; }

#include <random>
static float RandomFloat(float min, float max)
{
    static std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<float> dis(min, max);
    return dis(gen);
}

static float RandomInt(int min, int max)
{
    return min + rand() % (max - min + 1);
}

static int min_int(int a, int b)
{
    return std::min(a, b);
}

static float min_float(float a, float b)
{
    return std::min(a, b);
}

static int max_int(int a, int b)
{
    return std::max(a, b);
}

static float max_float(float a, float b)
{
    return std::max(a, b);
}

static float dot(const glm::vec3& v1, const glm::vec3& v2)
{
    return glm::dot(v1, v2);
}

static float distance(const glm::vec3& v1, const glm::vec3& v2)
{
    return glm::distance(v1, v2);
}

static float length(const glm::vec3& v)
{
    return glm::length(v);
}

void RegisterMath(asIScriptEngine* engine)
{
    int r;
    
    // 1. Регистрируем тип Vector3 (сопоставляем с glm::vec3)
    r = engine->RegisterObjectType("Vector3", sizeof(glm::vec3),
                                   asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_C | asOBJ_APP_CLASS_ALLFLOATS); assert(r >= 0);
    
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
    
    
    // Регистрируем поведение списка инициализации с паттерном "{float, float, float}"
    engine->RegisterObjectBehaviour("Vector3", asBEHAVE_LIST_CONSTRUCT, "void f(const int &in list) {float, float, float}",
                                    asFUNCTION(+[](const glm::vec3* listBuffer, void* memory) { new(memory) glm::vec3(*listBuffer); }), asCALL_CDECL_OBJLAST);
    
    
    // Vec3 * float (Объект Vec3 слева, значит OBJFIRST. В C++ функции: Vec3, затем float)
    engine->RegisterObjectMethod("Vector3", "Vector3 opMul(float) const",
                                 asFUNCTION(Vec3MulFloat_ObjFirst), asCALL_CDECL_OBJFIRST);
    
    // float * Vec3 (Объект Vec3 справа от знака *, но для AngelScript это opMul_r.
    // Флаг OBJLAST заставит AngelScript передать Vec3 ПОСЛЕДНИМ аргументом в C++ функцию: float, затем Vec3)
    engine->RegisterObjectMethod("Vector3", "Vector3 opMul_r(float) const",
                                 asFUNCTION(FloatMulVec3_ObjLast), asCALL_CDECL_OBJLAST);

    
    // --- 2. Операции МЕЖДУ векторами ---
    
    // Сложение (v1 + v2)
    r = engine->RegisterObjectMethod("Vector3", "Vector3 opAdd(const Vector3 &in) const",
                                     asFUNCTION(Vec3AddVec3), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    
    // Вычитание (v1 - v2)
    r = engine->RegisterObjectMethod("Vector3", "Vector3 opSub(const Vector3 &in) const",
                                     asFUNCTION(Vec3SubVec3), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    
    // Покомпонентное умножение (v1 * v2)
    r = engine->RegisterObjectMethod("Vector3", "Vector3 opMul(const Vector3 &in) const",
                                     asFUNCTION(Vec3MulVec3), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    
    // --- 3. Операции между векторами с присваиванием (+=, -=) ---
    
    // v1 += v2
    r = engine->RegisterObjectMethod("Vector3", "Vector3& opAddAssign(const Vector3 &in)",
                                     asFUNCTION(Vec3AddAssignVec3), asCALL_CDECL_OBJLAST); assert(r >= 0);
    
    // v1 -= v2
    r = engine->RegisterObjectMethod("Vector3", "Vector3& opSubAssign(const Vector3 &in)",
                                     asFUNCTION(Vec3SubAssignVec3), asCALL_CDECL_OBJLAST); assert(r >= 0);
    
    engine->RegisterGlobalFunction("float dot(const Vector3 &in, const Vector3 &in)", asFUNCTION(dot), asCALL_CDECL);
    engine->RegisterGlobalFunction("float distance(const Vector3 &in, const Vector3 &in)", asFUNCTION(distance), asCALL_CDECL);
    engine->RegisterGlobalFunction("float length(const Vector3 &in)", asFUNCTION(length), asCALL_CDECL);
    
    engine->SetDefaultNamespace("Math");
    engine->RegisterGlobalFunction("float RandomFloat(float, float)", asFUNCTION(RandomFloat), asCALL_CDECL);
    engine->RegisterGlobalFunction("float RandomInt(int, int)", asFUNCTION(RandomInt), asCALL_CDECL);
    engine->RegisterGlobalFunction("float atan2(float, float)", asFUNCTION(atan2f), asCALL_CDECL);
    engine->RegisterGlobalFunction("int min(int, int)", asFUNCTION(min_int), asCALL_CDECL);
    engine->RegisterGlobalFunction("int max(int, int)", asFUNCTION(max_int), asCALL_CDECL);
    engine->RegisterGlobalFunction("float min(float, float)", asFUNCTION(min_float), asCALL_CDECL);
    engine->RegisterGlobalFunction("float max(float, float)", asFUNCTION(max_float), asCALL_CDECL);
    engine->SetDefaultNamespace("");
}

void RegisterTransformComponent(asIScriptEngine* engine)
{
    int r;
    
    // Регистрируем TransformComponent. asOBJ_REF | asOBJ_NOCOUNT означает,
    // что AngelScript не управляет временем жизни этого C++ объекта (вы им управляете сами)
    r = engine->RegisterObjectType("TransformComponent", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    
    // Регистрируем позицию как свойство "position"
    r = engine->RegisterObjectMethod("TransformComponent", "Vector3 GetPosition() const", asMETHOD(TransformComponent, getPosition), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("TransformComponent", "void SetPosition(const Vector3 &in)", asMETHOD(TransformComponent, setPosition), asCALL_THISCALL); assert(r >= 0);
    
    r = engine->RegisterObjectMethod("TransformComponent", "Vector3 GetWorldPosition() const",
                                     asMETHOD(TransformComponent, getWorldPosition), asCALL_THISCALL); assert(r >= 0);
    
    // Скрываем кватернион! Регистрируем углы Эйлера как свойство "rotation"
    r = engine->RegisterObjectMethod("TransformComponent", "Vector3 GetRotation() const", asMETHOD(TransformComponent, getRotation), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("TransformComponent", "void SetRotation(const Vector3 &in)", asMETHOD(TransformComponent, setRotation), asCALL_THISCALL); assert(r >= 0);
    
    // FIXME: Сеттеры и геттеры (set_position и get_position) срабатывают некорректно для цепочек вида transform.position += velocity * dt;
    
    r = engine->RegisterObjectMethod("TransformComponent", "Vector3 GetForward()", asMETHOD(TransformComponent, getForward), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("TransformComponent", "Vector3 GetRight()", asMETHOD(TransformComponent, getRight), asCALL_THISCALL); assert(r >= 0);
}

static void PlaySound(const std::string& path)
{
    AudioSystem::Get().PlaySound(path);
}

void RegisterAudioComponent(asIScriptEngine* engine)
{
    // Регистрируем Entity как тип-ссылку (без подсчета ссылок)
    engine->RegisterObjectType("AudioComponent", 0, asOBJ_REF | asOBJ_NOCOUNT);
    
    // Регистрируем метод получения AudioComponent у Entity
    engine->RegisterObjectMethod("AudioComponent", "void Play()", asMETHOD(AudioComponent, play), asCALL_THISCALL);
    
    engine->RegisterGlobalFunction("void PlaySound(const string &in)", asFUNCTION(PlaySound), asCALL_CDECL);
}

void RegisterAnimatorComponent(asIScriptEngine* engine)
{
    // Регистрируем Entity как тип-ссылку (без подсчета ссылок)
    engine->RegisterObjectType("AnimatorComponent", 0, asOBJ_REF | asOBJ_NOCOUNT);
    
    // Регистрация методов очереди
    engine->RegisterObjectMethod("AnimatorComponent", "void Play(const string &in, bool, float = 0.0f)",
                                 asMETHODPR(AnimatorComponent, play, (const std::string&, bool, float), void), asCALL_THISCALL);
    
    engine->RegisterObjectMethod("AnimatorComponent", "void PlayQueue(const string &in, const string &in, bool, float = 0.2f)",
                                 asMETHODPR(AnimatorComponent, playQueue, (const std::string&, const std::string&, bool, float), void), asCALL_THISCALL);
    
    
    // Для Варианта А (коллбэки): регистрируем сигнатуру функции-делегата в AngelScript
    engine->RegisterFuncdef("void OnClipFinishedCallback(const string &in)");
    engine->RegisterObjectProperty("AnimatorComponent", "OnClipFinishedCallback@ onClipFinished", asOFFSET(AnimatorComponent, m_onClipFinishedCallback));
    
    engine->RegisterFuncdef("void OnAnimEventCallback(const string &in eventName, const string &in eventParam)");
    engine->RegisterObjectProperty("AnimatorComponent", "OnAnimEventCallback@ onAnimEvent", asOFFSET(AnimatorComponent, onAnimEvent));
    
    // Сигнатура функции, которая выполнится в момент нотифая
    engine->RegisterFuncdef("void OnAnimNotify()");
    
    // Регистрация метода в компонент
    engine->RegisterObjectMethod("AnimatorComponent", "void addNotify(const string &in clipName, float time, OnAnimNotify@ callback)",
                                 asMETHOD(AnimatorComponent, addNotify), asCALL_THISCALL);
    
    engine->RegisterObjectMethod("AnimatorComponent", "void clearNotifies(const string &in clipName)",
                                 asMETHOD(AnimatorComponent, clearNotifies), asCALL_THISCALL);
    
    engine->RegisterObjectMethod("AnimatorComponent", "void clearAllNotifies()",
                                 asMETHOD(AnimatorComponent, clearAllNotifies), asCALL_THISCALL);
    
    engine->RegisterObjectMethod("AnimatorComponent",
                                 "Vector3 getBoneWorldPosition(int boneIndex) const",
                                 asMETHOD(AnimatorComponent, getBoneWorldPosition),
                                 asCALL_THISCALL);

}

void RegisterMovementComponent(asIScriptEngine* engine)
{
    engine->RegisterObjectType("MovementComponent", 0, asOBJ_REF | asOBJ_NOCOUNT);
    
    engine->RegisterObjectMethod("MovementComponent", "void AddMovementInput(const Vector3 &in, float)",
                                 asMETHOD(MovementComponent, addMovementInput), asCALL_THISCALL);
    
    engine->RegisterObjectMethod("MovementComponent", "void Jump()",
                                 asMETHOD(MovementComponent, jump), asCALL_THISCALL);
    
    engine->RegisterObjectMethod("MovementComponent", "bool IsGrounded()",
                                 asMETHOD(MovementComponent, isGrounded), asCALL_THISCALL);
    
//    engine->RegisterObjectMethod("MovementComponent", "bool IsWalking()",
//                                 asMETHOD(MovementComponent, isWalking), asCALL_THISCALL);
}

// Посредник для установки коллбэка из скрипта
static void SetOnTriggerEnter(CollisionComponent* collider, asIScriptFunction* asFunc)
{
    if (!collider) return;
    
    if (asFunc == nullptr) {
        collider->onTriggerEnter = nullptr;
        return;
    }
    
    // Заворачиваем вызов AngelScript в std::function
    collider->onTriggerEnter = [asFunc](Entity* other) {
        asIScriptEngine* engine = asFunc->GetEngine();
        asIScriptContext* ctx = engine->CreateContext();
        ctx->Prepare(asFunc);
        ctx->SetArgAddress(0, other);
        ctx->Execute();
        ctx->Release();
    };
}

void RegisterCollisionComponent(asIScriptEngine* engine)
{
    engine->RegisterObjectType("CollisionComponent", 0, asOBJ_REF | asOBJ_NOCOUNT);
    engine->RegisterObjectProperty("CollisionComponent", "bool enabled", asOFFSET(CollisionComponent, enabled));
}

void RegisterEntity(asIScriptEngine* engine)
{
    // Регистрируем Entity как тип-ссылку (без подсчета ссылок)
    engine->RegisterObjectType("Entity", 0, asOBJ_REF | asOBJ_NOCOUNT);
    
    engine->RegisterObjectProperty("Entity", "bool active", asOFFSET(Entity, m_isActive));
    
    // Регистрируем метод получения TransformComponent у Entity
    engine->RegisterObjectMethod("Entity", "TransformComponent@ GetTransform()",
                                 asMETHOD(Entity, getComponent<TransformComponent>), asCALL_THISCALL);
    
    engine->RegisterObjectMethod("Entity", "AudioComponent@ GetAudio()",
                                 asMETHOD(Entity, getComponent<AudioComponent>), asCALL_THISCALL);
    
    engine->RegisterObjectMethod("Entity", "AnimatorComponent@ GetAnimator()",
                                 asMETHOD(Entity, getComponent<AnimatorComponent>), asCALL_THISCALL);
    
    engine->RegisterObjectMethod("Entity", "MovementComponent@ GetMovement()",
                                 asMETHOD(Entity, getComponent<MovementComponent>), asCALL_THISCALL);
    
    engine->RegisterObjectMethod("Entity", "CollisionComponent@ GetCollider()",
                                 asMETHOD(Entity, getComponent<CollisionComponent>), asCALL_THISCALL);
    
    engine->RegisterObjectMethod("Entity", "const string& GetName() const",
                                 asMETHOD(Entity, GetName), asCALL_THISCALL);
    
    
    
    engine->RegisterFuncdef("void OnTriggerEnterCallback(Entity@ other)");
    //engine->RegisterFuncdef("void OnCollisionEnter(Entity@ other)");
    
//    engine->RegisterObjectMethod("CollisionComponent", "void SetOnTriggerEnter(OnTriggerEnterCallback@)", asFUNCTION(SetOnTriggerEnter), asCALL_THISCALL);
    engine->RegisterGlobalFunction("float SetOnTriggerEnter(CollisionComponent@, OnTriggerEnterCallback@)", asFUNCTION(SetOnTriggerEnter), asCALL_CDECL);
    
//    engine->RegisterObjectProperty("CollisionComponent", "OnTriggerEnterCallback@ onTriggerEnter", asOFFSET(CollisionComponent, onTriggerEnterScript));
    //engine->RegisterObjectProperty("CollisionComponent", "OnCollisionEnterCallback@ onCollisionEnter", asOFFSET(CollisionComponent, onCollisionEnterScript));
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
    r = engine->RegisterEnumValue("KeyCode", "R",        82);  assert(r >= 0);
    r = engine->RegisterEnumValue("KeyCode", "C",        67);  assert(r >= 0);
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
    r = engine->RegisterGlobalFunction("float GetMouseOffsetX()", asFUNCTION(Input::getMouseOffsetX), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("float GetMouseOffsetY()", asFUNCTION(Input::getMouseOffsetY), asCALL_CDECL); assert(r >= 0);
    
    r = engine->RegisterGlobalFunction("bool GetLeftMouseButtonPressed()", asFUNCTION(Input::isLeftMouseButtonPressed), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("bool GetLeftMouseButtonClicked()", asFUNCTION(Input::isLeftMouseButtonClicked), asCALL_CDECL); assert(r >= 0);
    
    r = engine->RegisterGlobalFunction("bool GetRightMouseButton()", asFUNCTION(Input::isRightMouseButtonPressed), asCALL_CDECL); assert(r >= 0);
    
    r = engine->RegisterGlobalFunction("void SetCursorEnabled(bool state)", asFUNCTION(Input::setCursorEnabled), asCALL_CDECL); assert(r >= 0);
    
    // Сбрасываем пространство имен обратно в глобальное, чтобы не сломать регистрацию других систем
    r = engine->SetDefaultNamespace(""); assert(r >= 0);
}

static void AddBillboardShort(int textureId, glm::vec3 position, float scale, float zRot)
{
    SpriteRenderer::AddBillboard(textureId, position, scale, zRot);
}

void RegisterSpriteRenderer(asIScriptEngine* engine)
{
    engine->SetDefaultNamespace("SpriteRenderer");
    engine->RegisterGlobalFunction("int preloadTexture(const string &in)", asFUNCTION(SpriteRenderer::PreloadTexture), asCALL_CDECL);
    engine->RegisterGlobalFunction("void AddBillboard(int, Vector3, float, float)", asFUNCTION(AddBillboardShort), asCALL_CDECL);
    engine->SetDefaultNamespace("");
    
    // Регистрируем функцию в глобальном пространстве AngelScript
    engine->RegisterGlobalFunction("void SpawnSparks(const Vector3 &in position, const Vector3 &in normal, int count)",
                                   asFUNCTION(ParticleSystem::SpawnSparks),
                                   asCALL_CDECL);
    
    engine->RegisterGlobalFunction("void SpawnBlood(const Vector3 &in position, const Vector3 &in normal, int count)",
                                   asFUNCTION(ParticleSystem::SpawnBlood), asCALL_CDECL);
    
    engine->RegisterGlobalFunction("void SpawnDust(const Vector3 &in position, int count)",
                                   asFUNCTION(ParticleSystem::SpawnDust), asCALL_CDECL);
    
    engine->RegisterGlobalFunction("void SpawnSmoke(const Vector3 &in position, int count)",
                                   asFUNCTION(ParticleSystem::SpawnSmoke), asCALL_CDECL);
    
    engine->RegisterGlobalFunction("void SpawnTracer(const Vector3 &in start, const Vector3 &in end)",
                                   asFUNCTION(ParticleSystem::SpawnTracer), asCALL_CDECL);
    
    
    engine->SetDefaultNamespace("DecalManager");
    engine->RegisterGlobalFunction("void AddDecal(int textureId, Vector3 position, Vector3 normal, float size, float lifespan)",
                                   asFUNCTION(DecalManager::AddDecal), asCALL_CDECL);
    engine->SetDefaultNamespace("");
}

static void ConstructLayout(void* memory) {
    new(memory) HUD::Layout();
}

static void CopyConstructLayout(const HUD::Layout& other, void* memory) {
    new(memory) HUD::Layout(other);
}

static void DestructLayout(void* memory) {
    reinterpret_cast<HUD::Layout*>(memory)->~Layout();
}

void RegisterUI(asIScriptEngine* engine)
{
    engine->RegisterObjectType("Color", sizeof(glm::vec4),
                               asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_C | asOBJ_APP_CLASS_ALLFLOATS);
    
    engine->RegisterObjectProperty("Color", "float r", asOFFSET(glm::vec4, x));
    engine->RegisterObjectProperty("Color", "float g", asOFFSET(glm::vec4, y));
    engine->RegisterObjectProperty("Color", "float b", asOFFSET(glm::vec4, z));
    engine->RegisterObjectProperty("Color", "float a", asOFFSET(glm::vec4, w));
    
    engine->RegisterObjectBehaviour("Color", asBEHAVE_CONSTRUCT, "void f()",
                                    asFUNCTION(+[](void* memory) { new(memory) glm::vec4(1.0f); }), asCALL_CDECL_OBJLAST);
    
    engine->RegisterObjectBehaviour("Color", asBEHAVE_CONSTRUCT, "void f(float, float, float, float)",
                                    asFUNCTION(+[](float r, float g, float b, float a, void* memory) { new(memory) glm::vec4(r, g, b, a); }), asCALL_CDECL_OBJLAST);
    
    
    
    engine->RegisterObjectType("Vector2", sizeof(glm::vec2),
                               asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_C | asOBJ_APP_CLASS_ALLFLOATS);
    
    engine->RegisterObjectProperty("Vector2", "float x", asOFFSET(glm::vec2, x));
    engine->RegisterObjectProperty("Vector2", "float y", asOFFSET(glm::vec2, y));
    
    engine->RegisterObjectBehaviour("Vector2", asBEHAVE_CONSTRUCT, "void f()",
                                    asFUNCTION(+[](void* memory) { new(memory) glm::vec2(0.0f); }), asCALL_CDECL_OBJLAST);
    
    engine->RegisterObjectBehaviour("Vector2", asBEHAVE_CONSTRUCT, "void f(float, float)",
                                    asFUNCTION(+[](float x, float y, void* memory) { new(memory) glm::vec2(x, y); }), asCALL_CDECL_OBJLAST);
    
    engine->SetDefaultNamespace("UI");
    
    engine->RegisterObjectType("Layout", sizeof(HUD::Layout), asOBJ_VALUE | asOBJ_APP_CLASS_C | asOBJ_APP_CLASS_ALLFLOATS);
    
    engine->RegisterObjectBehaviour("Layout", asBEHAVE_CONSTRUCT, "void f()",
                                    asFUNCTION(ConstructLayout), asCALL_CDECL_OBJLAST);
    
    // Регистрируем конструктор копирования (критически важен для возврата из функций по значению!)
    engine->RegisterObjectBehaviour("Layout", asBEHAVE_CONSTRUCT, "void f(const Layout &in)",
                                    asFUNCTION(CopyConstructLayout), asCALL_CDECL_OBJLAST);
    
    // Регистрируем деструктор
    engine->RegisterObjectBehaviour("Layout", asBEHAVE_DESTRUCT, "void f()",
                                    asFUNCTION(DestructLayout), asCALL_CDECL_OBJLAST);
    
//    engine->RegisterObjectBehaviour("Layout", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(+[](void* memory) { new(memory) HUD::Layout(); }), asCALL_CDECL_OBJLAST);
    
    engine->RegisterGlobalFunction("Layout LayoutTopLeft(float x, float y, float width, float height)", asFUNCTION(HUD::Layout::TopLeft), asCALL_CDECL);
    engine->RegisterGlobalFunction("Layout LayoutTopRight(float x, float y, float width, float height)", asFUNCTION(HUD::Layout::TopRight), asCALL_CDECL);
    engine->RegisterGlobalFunction("Layout LayoutBottomLeft(float x, float y, float width, float height)", asFUNCTION(HUD::Layout::BottomLeft), asCALL_CDECL);
    engine->RegisterGlobalFunction("Layout LayoutBottomRight(float x, float y, float width, float height)", asFUNCTION(HUD::Layout::BottomRight), asCALL_CDECL);
    engine->RegisterGlobalFunction("Layout LayoutCenter(float width, float height, Vector2 offset = Vector2())", asFUNCTION(HUD::Layout::Center), asCALL_CDECL);
    engine->RegisterGlobalFunction("Layout LayoutFullscreen(float margin = 0)", asFUNCTION(HUD::Layout::FullScreen), asCALL_CDECL);
    

    engine->RegisterGlobalFunction("void DrawRect(const Layout &in layout, const Color &in color)",
                                   asFUNCTIONPR(HUD::DrawRect, (const HUD::Layout&, const glm::vec4&), void), asCALL_CDECL);
    
    engine->RegisterGlobalFunction("void DrawText(const Layout &in layout, const string &in text, const Color &in color)",
                                   asFUNCTIONPR(HUD::DrawText, (const HUD::Layout&, const std::string&, const glm::vec4&), void), asCALL_CDECL);
    
    engine->RegisterGlobalFunction("void DrawImage(const Layout &in layout, int id, const Color &in color)",
                                   asFUNCTIONPR(HUD::DrawTexture, (const HUD::Layout&, unsigned int, const glm::vec4&), void), asCALL_CDECL);
    
    engine->SetDefaultNamespace("");
}

#include "PhysicsCommon.h"

namespace {
    struct RaycastResult
    {
        glm::vec3 point;
        glm::vec3 normal;
        Entity* entity;
    };
    
    bool MakeRaycast(const glm::vec3& start, const glm::vec3& dir, float distance, RaycastResult& hit, Entity* ignore)
    {
        HitResult result;
        result.endpos = { 0, 0, 0 };
        result.normal = { 0, 1, 0 };
        
        uint32_t mask = LAYER_PLAYER | LAYER_ENEMY | LAYER_INTERACTIVE;
        
        g_currentScene->trace(result, OpenGL2Quake(start), OpenGL2Quake(start + dir * distance), glm::vec3(0), glm::vec3(0), ignore, mask);
        
        hit.point = Quake2OpenGL(result.endpos);
        hit.normal = Quake2OpenGL(result.normal);
        hit.entity = result.hitEntity;
        
        return result.fraction > 0.0f && result.fraction < 1.0f;
    }
}

void RegisterPhysics(asIScriptEngine* engine)
{
    int r;
    
    // 1. Регистрируем тип RaycastResult
    r = engine->RegisterObjectType("RaycastResult", sizeof(RaycastResult),
                                   asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_C | asOBJ_APP_CLASS_ALLFLOATS); assert(r >= 0);
    
    // 2. Регистрируем свойства
    r = engine->RegisterObjectProperty("RaycastResult", "Vector3 point", asOFFSET(RaycastResult, point)); assert(r >= 0);
    r = engine->RegisterObjectProperty("RaycastResult", "Vector3 normal", asOFFSET(RaycastResult, normal)); assert(r >= 0);
    r = engine->RegisterObjectProperty("RaycastResult", "Entity@ entity", asOFFSET(RaycastResult, entity)); assert(r >= 0);
    
    engine->SetDefaultNamespace("Physics");
    engine->RegisterGlobalFunction("bool Raycast(const Vector3 &in start, const Vector3 &in dir, float distance, RaycastResult &out hit, Entity &in ignore)",
                                   asFUNCTION(MakeRaycast), asCALL_CDECL);
    engine->SetDefaultNamespace("");
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
