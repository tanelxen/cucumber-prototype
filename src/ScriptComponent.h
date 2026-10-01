//
//  ScriptComponent.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 02.09.26.
//

#pragma once

#include "Entity.h"
#include <angelscript.h>
#include <string>

#include "../vendor/json.hpp"

class ScriptComponent : public Component
{
public:
    ScriptComponent();
    ~ScriptComponent();
    
    std::string scriptPath;
    bool m_isInitialized = false;
    bool m_isPlaying = true;
    
    std::string m_ClassName;
    
    asIScriptObject* m_ScriptObject = nullptr;
    asIScriptFunction* m_StartMethod = nullptr;
    asIScriptFunction* m_UpdateMethod = nullptr;
    
    nlohmann::json m_StateSnapshot;
    
    const std::string getTypeName() const override { return "ScriptComponent"; }
    
    void init(const std::string& className);
    
    // Методы для Hot Reload
    void saveStateSnapshot();      // Вызывается ДО перекомпиляции
    void loadStateSnapshot();      // Вызывается ПОСЛЕ перекомпиляции
    void recreateScriptObject();   // Пересоздает asIScriptObject из нового модуля
    
    void awake() override;
    
    void start() override;
    void update(float dt) override;
    
    void save(nlohmann::json& j) const override;
    void load(const nlohmann::json& j) override;
    
    void drawProperties();
    
private:
    void getStateSnapshot(nlohmann::json& snapshot) const;
    void loadScriptStateFromValues(const nlohmann::json& snapshot);
};
