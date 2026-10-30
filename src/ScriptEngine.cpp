#include "ScriptEngine.h"

#include "ScriptComponent.h"
#include "SceneManager.h"
#include "World.h"

namespace
{
void ReleaseScriptInstances(WorldScene& scene)
{
    for (const auto& entity : scene.m_entities)
    {
        for (ScriptComponent* script : entity->getComponents<ScriptComponent>())
            script->releaseScriptInstance();
    }
}

bool RecreateScriptInstances(WorldScene& scene)
{
    bool success = true;
    for (const auto& entity : scene.m_entities)
    {
        for (ScriptComponent* script : entity->getComponents<ScriptComponent>())
            success = script->recreateScriptObject() && success;
    }
    return success;
}

void RestoreScriptSnapshots(WorldScene& scene)
{
    for (const auto& entity : scene.m_entities)
    {
        for (ScriptComponent* script : entity->getComponents<ScriptComponent>())
            script->loadStateSnapshot();
    }
}
}

bool ScriptEngine::init(const std::string& scriptsFolder, const ScriptRuntimeOptions& options)
{
    return GetScriptRuntime().initialize(scriptsFolder, options);
}

bool ScriptEngine::CompileAllScripts(const std::string& scriptsFolder)
{
    return GetScriptRuntime().compileAllScripts(scriptsFolder);
}

void ScriptEngine::HotReloadAllScripts(const std::string& scriptsFolder)
{
    ClearLogs();
    WorldScene* activeScene = SceneManager::Get().getActiveScene();
    if (!activeScene) return;

    if (!GetScriptRuntime().prepareReload(scriptsFolder))
    {
        AddLogMessage({{}, 0, 0,
                       "Hot reload compilation failed; active scripts were left unchanged",
                       ScriptLogType::Error});
        return;
    }

    for (const auto& entity : activeScene->m_entities)
        for (ScriptComponent* script : entity->getComponents<ScriptComponent>())
            script->saveStateSnapshot();

    ReleaseScriptInstances(*activeScene);

    if (!GetScriptRuntime().commitReload())
    {
        GetScriptRuntime().cancelReload();
        const bool restored = RecreateScriptInstances(*activeScene);
        RestoreScriptSnapshots(*activeScene);
        AddLogMessage({{}, 0, 0, "Hot reload could not activate the compiled scripts",
                       ScriptLogType::Error});
        if (!restored)
            AddLogMessage({{}, 0, 0, "Failed to recreate one or more previous script instances",
                           ScriptLogType::Error});
        return;
    }

    if (!RecreateScriptInstances(*activeScene))
    {
        ReleaseScriptInstances(*activeScene);
        const bool rolledBack = GetScriptRuntime().rollbackReload();
        const bool restored = rolledBack && RecreateScriptInstances(*activeScene);
        if (restored)
        {
            RestoreScriptSnapshots(*activeScene);
        }
        AddLogMessage({{}, 0, 0,
                       restored
                           ? "Hot reload failed to create new script instances; previous scripts were restored"
                           : "Hot reload failed to create new script instances and could not fully restore the previous scripts",
                       ScriptLogType::Error});
        return;
    }

    RestoreScriptSnapshots(*activeScene);
    if (!GetScriptRuntime().finalizeReload())
    {
        ReleaseScriptInstances(*activeScene);
        const bool rolledBack = GetScriptRuntime().rollbackReload();
        const bool restored = rolledBack && RecreateScriptInstances(*activeScene);
        if (restored)
        {
            RestoreScriptSnapshots(*activeScene);
        }
        AddLogMessage({{}, 0, 0,
                       restored
                           ? "Hot reload could not finalize; previous scripts were restored"
                           : "Hot reload could not finalize and could not fully restore the previous scripts",
                       ScriptLogType::Error});
    }
}
