//
//  ScriptComponent.hpp
//  Cucumber
//
//  Created by Fedor Artemenkov on 02.09.26.
//

#pragma once

#include "Entity.h"
#include "ScriptRuntime.h"

#include <memory>
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
    nlohmann::json m_StateSnapshot;
    
    const std::string getTypeName() const override { return "ScriptComponent"; }
    
    void init(const std::string& className);
    
    // Методы для Hot Reload
    void saveStateSnapshot();
    void releaseScriptInstance();
    void loadStateSnapshot();      // Вызывается ПОСЛЕ перекомпиляции
    bool recreateScriptObject();
    
    void awake() override;
    
    void start() override;
    void update(float dt) override;
    
    void save(nlohmann::json& j) const override;
    void load(const nlohmann::json& j) override;
    
    IScriptInstance* getScriptInstance() const { return m_scriptInstance.get(); }
    const std::vector<ScriptPropertyDescriptor>& getScriptProperties() const;
    nlohmann::json getPropertyValue(const std::string& name) const;
    bool setPropertyValue(const std::string& name, const nlohmann::json& value);
    bool setPropertyReference(const std::string& name, Entity* target);
    bool clearPropertyReference(const std::string& name);
    
private:
    nlohmann::json getStateSnapshot() const;
    void loadScriptStateFromValues(const nlohmann::json& snapshot);
    std::unique_ptr<IScriptInstance> m_scriptInstance;
};
