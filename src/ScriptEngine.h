//
//  ScriptEngine.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 03.09.26.
//

#pragma once

#include <angelscript.h>
#include <vector>
#include <string>

enum class ScriptLogType {
    Info,
    Warning,
    Error,
    ScriptPrint
};

struct ScriptLogMessage {
    std::string section; // Имя файла (например, "assets/scripts/player.as")
    int row;            // Строка ошибки
    int col;            // Колонка ошибки
    std::string text;   // Текст сообщения
    ScriptLogType type; // Тип (Info/Warning/Error)
};

class ScriptEngine
{
public:
//    ScriptEngine();
//    ~ScriptEngine();
    
    static ScriptEngine& get()
    {
        static ScriptEngine instance;
        return instance;
    }
    
    bool init();
    bool CompileAllScripts(const std::string& scriptsFolder);
    void HotReloadAllScripts(const std::string& scriptsFolder);
    
//    bool LoadModule(const std::string& moduleName, const std::string& scriptPath);
    
    std::vector<std::string> GetMetadataForTypeProperty(int typeId, int varIdx);
    
    asIScriptEngine* GetRawEngine() { return m_asEngine; }
    asIScriptContext* GetContext() { return m_Context; }
    
    
    // Добавляем доступ к вектору логов
    const std::vector<ScriptLogMessage>& GetLogMessages() const { return m_LogMessages; }
    void ClearLogs() { m_LogMessages.clear(); }
    
    // Сделаем метод добавления лога публичным, чтобы статический callback мог его вызвать
    void AddLogMessage(const ScriptLogMessage& msg) { m_LogMessages.push_back(msg); }
    
private:
    ScriptEngine() = default;
    
    void RegisterEngineAPI();
    static void MessageCallback(const asSMessageInfo* msg, void* param);
    
    asIScriptEngine* m_asEngine = nullptr;
    asIScriptContext* m_Context = nullptr;
    
    std::vector<ScriptLogMessage> m_LogMessages;
};
