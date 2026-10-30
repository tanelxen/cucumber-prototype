//
//  ScriptEngine.h
//  Cucumber
//
//  Created by Fedor Artemenkov on 03.09.26.
//

#pragma once

#include "ScriptRuntime.h"

#include <vector>
#include <string>
#include <iostream>

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
    
    bool init(const std::string& scriptsFolder,
              const ScriptRuntimeOptions& options = ScriptRuntimeOptions{});
    bool CompileAllScripts(const std::string& scriptsFolder);
    void HotReloadAllScripts(const std::string& scriptsFolder);
    
//    bool LoadModule(const std::string& moduleName, const std::string& scriptPath);
    
    // Добавляем доступ к вектору логов
    const std::vector<ScriptLogMessage>& GetLogMessages() const { return m_LogMessages; }
    void ClearLogs() { m_LogMessages.clear(); }
    
    // Сделаем метод добавления лога публичным, чтобы статический callback мог его вызвать
    void AddLogMessage(const ScriptLogMessage& msg) {
        m_LogMessages.push_back(msg);
        std::cout << msg.text << std::endl;
    }
    
private:
    ScriptEngine() = default;
    
    std::vector<ScriptLogMessage> m_LogMessages;
};
