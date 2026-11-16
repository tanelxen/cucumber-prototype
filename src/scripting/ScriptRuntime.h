#pragma once

#include "../vendor/json.hpp"

#include <memory>
#include <string>
#include <vector>

class Entity;

enum class ScriptPropertyKind
{
    Float,
    Integer,
    Boolean,
    String,
    Reference,
    Unsupported
};

struct ScriptPropertyDescriptor
{
    std::string name;
    std::string typeName;
    ScriptPropertyKind kind = ScriptPropertyKind::Unsupported;
    bool hidden = false;
    std::string header;
    std::string tooltip;
    bool hasRange = false;
    float rangeMin = 0.0f;
    float rangeMax = 1.0f;
};

struct ScriptRuntimeOptions
{
    bool sceneApi = true;
    bool inputApi = true;
    bool renderingApi = true;
    bool uiApi = true;
    bool physicsApi = true;
    bool generateEditorDefinitions = true;

    static ScriptRuntimeOptions standalone()
    {
        return {true, true, true, false, true, false};
    }
};

class IScriptInstance
{
public:
    virtual ~IScriptInstance() = default;

    virtual const std::string& getClassName() const = 0;
    virtual const std::vector<ScriptPropertyDescriptor>& getProperties() const = 0;
    virtual bool invokeStart() = 0;
    virtual bool invokeUpdate(float dt) = 0;
    virtual nlohmann::json getPropertyValue(const std::string& name) const = 0;
    virtual bool setPropertyValue(const std::string& name, const nlohmann::json& value) = 0;
    virtual Entity* getPropertyReference(const std::string& name) const = 0;
    virtual bool setPropertyReference(const std::string& name, Entity* target) = 0;
    virtual bool clearPropertyReference(const std::string& name) = 0;
};

class IScriptRuntime
{
public:
    virtual ~IScriptRuntime() = default;

    virtual bool initialize(const std::string& scriptsFolder,
                            const ScriptRuntimeOptions& options) = 0;
    virtual bool compileAllScripts(const std::string& scriptsFolder) = 0;
    virtual bool prepareReload(const std::string& scriptsFolder) = 0;
    virtual bool commitReload() = 0;
    virtual bool finalizeReload() = 0;
    virtual bool rollbackReload() = 0;
    virtual void cancelReload() = 0;
    virtual std::unique_ptr<IScriptInstance> createInstance(
        const std::string& className, Entity* owner, std::string& scriptPath) = 0;
    virtual std::vector<std::string> getAvailableScriptClasses() const = 0;
};

IScriptRuntime& GetScriptRuntime();
