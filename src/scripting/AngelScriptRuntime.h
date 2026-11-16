#pragma once

#include "ScriptRuntime.h"

#include <angelscript.h>
#include <scriptbuilder/scriptbuilder.h>
#include <scripthandle/scripthandle.h>

#include <array>
#include <cstdint>
#include <vector>

class AngelScriptRuntime final : public IScriptRuntime
{
public:
    ~AngelScriptRuntime() override;

    bool initialize(const std::string& scriptsFolder,
                    const ScriptRuntimeOptions& options) override;
    bool compileAllScripts(const std::string& scriptsFolder) override;
    bool prepareReload(const std::string& scriptsFolder) override;
    bool commitReload() override;
    bool finalizeReload() override;
    bool rollbackReload() override;
    void cancelReload() override;
    std::unique_ptr<IScriptInstance> createInstance(
        const std::string& className, Entity* owner, std::string& scriptPath) override;
    std::vector<std::string> getAvailableScriptClasses() const override;

    asIScriptEngine* getEngine() const { return m_engine; }
    asIScriptContext* getContext() const { return m_context; }
    std::vector<std::string> getPropertyMetadata(int typeId, int propertyIndex);

    static asIScriptObject* FindScriptObject(Entity* entity, const std::string& className);
    static CScriptHandle GetScriptOnEntity(Entity* entity, const std::string& className);

private:
    void registerEngineApi(const ScriptRuntimeOptions& options);
    static void messageCallback(const asSMessageInfo* message, void* param);
    bool buildModule(CScriptBuilder& builder, const std::string& moduleName,
                     const std::string& scriptsFolder);

    asIScriptEngine* m_engine = nullptr;
    asIScriptContext* m_context = nullptr;
    ScriptRuntimeOptions m_options;
    std::array<CScriptBuilder, 2> m_builders;
    size_t m_activeBuilderIndex = 0;
    std::string m_activeModuleName = "Game";
    std::string m_pendingModuleName;
    size_t m_previousBuilderIndex = 0;
    std::string m_previousModuleName;
    bool m_reloadCommitted = false;
    uint64_t m_reloadGeneration = 0;
    std::vector<std::string> m_retiredModuleNames;
};

AngelScriptRuntime& GetAngelScriptRuntime();
