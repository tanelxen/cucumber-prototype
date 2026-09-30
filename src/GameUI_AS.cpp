//
//  AngelScriptUI.cpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 30.09.26.
//

#include "GameUI_AS.h"
#include "GameUI.h"

#include <angelscript.h>
#include <iostream>

static void RegisterColor(asIScriptEngine* engine)
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
}

static void RegisterVector2(asIScriptEngine* engine)
{
    engine->RegisterObjectType("Vector2", sizeof(glm::vec2),
                               asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_C | asOBJ_APP_CLASS_ALLFLOATS);
    
    engine->RegisterObjectProperty("Vector2", "float x", asOFFSET(glm::vec2, x));
    engine->RegisterObjectProperty("Vector2", "float y", asOFFSET(glm::vec2, y));
    
    engine->RegisterObjectBehaviour("Vector2", asBEHAVE_CONSTRUCT, "void f()",
                                    asFUNCTION(+[](void* memory) { new(memory) glm::vec2(0.0f); }), asCALL_CDECL_OBJLAST);
    
    engine->RegisterObjectBehaviour("Vector2", asBEHAVE_CONSTRUCT, "void f(float, float)",
                                    asFUNCTION(+[](float x, float y, void* memory) { new(memory) glm::vec2(x, y); }), asCALL_CDECL_OBJLAST);
}

static void ConstructWidgetDef(void* memory) {
    new(memory) GameUI::WidgetDefinition();
}

static void CopyConstructWidgetDef(const GameUI::WidgetDefinition& other, void* memory) {
    new(memory) GameUI::WidgetDefinition(other);
}

static void DestructWidgetDef(void* memory) {
    reinterpret_cast<GameUI::WidgetDefinition*>(memory)->~WidgetDefinition();
}

void RegisterGameUI(asIScriptEngine* engine)
{
    RegisterColor(engine);
    RegisterVector2(engine);
    
    // Устанавливаем дефолтное пространство имен "UI"
    engine->SetDefaultNamespace("UI");
    
    // 1. Регистрируем тип WidgetDefinition как Value-тип
    // Мы используем asOBJ_APP_CLASS_CDAK, так как у структуры есть конструктор,
    // конструктор копирования и деструктор (из-за std::string text внутри).
    int r = engine->RegisterObjectType("Widget", sizeof(GameUI::WidgetDefinition),
                                       asOBJ_VALUE | asOBJ_APP_CLASS_CDAK);
    if (r < 0) {
        std::cerr << "Ошибка регистрации типа Widget: " << r << std::endl;
        return;
    }
    
    // Регистрируем поведение управления памятью
    engine->RegisterObjectBehaviour("Widget", asBEHAVE_CONSTRUCT, "void f()",
                                    asFUNCTION(ConstructWidgetDef), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectBehaviour("Widget", asBEHAVE_CONSTRUCT, "void f(const Widget &in)",
                                    asFUNCTION(CopyConstructWidgetDef), asCALL_CDECL_OBJLAST);
    engine->RegisterObjectBehaviour("Widget", asBEHAVE_DESTRUCT, "void f()",
                                    asFUNCTION(DestructWidgetDef), asCALL_CDECL_OBJLAST);
    
    // 2. Регистрируем методы цепочки (Method Chaining / Fluent API)
    
    engine->RegisterObjectMethod("Widget", "Widget& TopLeft(float x, float y, float w, float h)",
                                 asMETHOD(GameUI::WidgetDefinition, TopLeft), asCALL_THISCALL);
    
    engine->RegisterObjectMethod("Widget", "Widget& TopRight(float x, float y, float w, float h)",
                                 asMETHOD(GameUI::WidgetDefinition, TopRight), asCALL_THISCALL);
    
    engine->RegisterObjectMethod("Widget", "Widget& BottomLeft(float x, float y, float w, float h)",
                                 asMETHOD(GameUI::WidgetDefinition, BottomLeft), asCALL_THISCALL);
    
    engine->RegisterObjectMethod("Widget", "Widget& BottomRight(float x, float y, float w, float h)",
                                 asMETHOD(GameUI::WidgetDefinition, BottomRight), asCALL_THISCALL);
    
    // Предполагаем, что Vector2 уже зарегистрирован в системе. Если нет, замените аргумент на "float offsetX = 0, float offsetY = 0"
    engine->RegisterObjectMethod("Widget", "Widget& Center(float w, float h, Vector2 offset = Vector2())",
                                 asMETHOD(GameUI::WidgetDefinition, Center), asCALL_THISCALL);
    
    engine->RegisterObjectMethod("Widget", "Widget& FullScreen()",
                                 asMETHOD(GameUI::WidgetDefinition, FullScreen), asCALL_THISCALL);
    
    // Метод установки цвета (предполагаем, что тип Color — это ваш glm::vec4)
    engine->RegisterObjectMethod("Widget", "Widget& SetColor(const Color &in c)",
                                 asMETHOD(GameUI::WidgetDefinition, SetColor), asCALL_THISCALL);
    
    // Метод установки Pivot вручную
    engine->RegisterObjectMethod("Widget", "Widget& SetPivot(const Vector2 &in p)",
                                 asMETHOD(GameUI::WidgetDefinition, SetPivot), asCALL_THISCALL);
    
    // Терминальный (конечный) метод, который осуществляет саму отрисовку
    engine->RegisterObjectMethod("Widget", "void Draw()",
                                 asMETHOD(GameUI::WidgetDefinition, Draw), asCALL_THISCALL);
    
    
    // 3. Регистрируем Глобальные Фабричные функции
    // Перегрузка с типом Color() по умолчанию сработает, если Color зарегистрирован.
    engine->RegisterGlobalFunction("Widget Rect(const Color &in color)",
                                   asFUNCTION(GameUI::Rect), asCALL_CDECL);
    
    engine->RegisterGlobalFunction("Widget Image(uint textureId)",
                                   asFUNCTION(GameUI::Image), asCALL_CDECL);
    
    engine->RegisterGlobalFunction("Widget Text(const string &in text, const Color &in color = Color(1,1,1,1))",
                                   asFUNCTION(GameUI::Text), asCALL_CDECL);
    
    // Сбрасываем namespace, чтобы не загрязнять глобальное окружение скриптов
    engine->SetDefaultNamespace("");
}
