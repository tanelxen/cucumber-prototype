#include "ScriptComponent.h"

#include "World.h"

#include <iostream>

ScriptComponent::ScriptComponent() : Component(Flags::RequiresUpdate | Flags::RequiresFixedUpdate)
{
}

void ScriptComponent::init(const std::string& className)
{
    m_scriptInstance.reset();
    m_isInitialized = false;
    m_ClassName = className;
    scriptPath.clear();

    m_scriptInstance = GetScriptRuntime().createInstance(className, owner, scriptPath);
    m_isInitialized = static_cast<bool>(m_scriptInstance);
    if (!m_isInitialized)
        std::cerr << "Failed to create script instance for class " << className << std::endl;
}

void ScriptComponent::awake()
{
    loadStateSnapshot();
}

void ScriptComponent::start()
{
    if (m_scriptInstance) m_scriptInstance->invokeStart();
}

void ScriptComponent::update(float dt)
{
    if (m_scriptInstance && m_isPlaying) m_scriptInstance->invokeUpdate(dt);
}

ScriptComponent::~ScriptComponent() = default;

bool ScriptComponent::recreateScriptObject()
{
    init(m_ClassName);
    return m_isInitialized;
}

const std::vector<ScriptPropertyDescriptor>& ScriptComponent::getScriptProperties() const
{
    static const std::vector<ScriptPropertyDescriptor> empty;
    return m_scriptInstance ? m_scriptInstance->getProperties() : empty;
}

nlohmann::json ScriptComponent::getPropertyValue(const std::string& name) const
{
    if (!m_scriptInstance) return nullptr;
    for (const ScriptPropertyDescriptor& property : m_scriptInstance->getProperties())
    {
        if (property.name != name) continue;
        if (property.kind != ScriptPropertyKind::Reference)
            return m_scriptInstance->getPropertyValue(name);

        Entity* target = m_scriptInstance->getPropertyReference(name);
        if (!target) return nullptr;

        nlohmann::json value = {{"type", property.typeName}, {"display", target->getName()}};
        if (owner && owner->isPrefabInstance() && !owner->isPrefabRoot)
            value["path"] = target->getPathRelativeTo(owner);
        else if (property.typeName == "Entity")
            value["uid"] = target->uid;
        else
            value["owner"] = target->uid;
        return value;
    }
    return nullptr;
}

bool ScriptComponent::setPropertyValue(const std::string& name, const nlohmann::json& value)
{
    if (!m_scriptInstance) return false;
    for (const ScriptPropertyDescriptor& property : m_scriptInstance->getProperties())
    {
        if (property.name != name) continue;
        if (property.kind != ScriptPropertyKind::Reference)
            return m_scriptInstance->setPropertyValue(name, value);
        if (value.is_null())
            return m_scriptInstance->clearPropertyReference(name);
        if (!value.is_object()) return false;

        Entity* target = nullptr;
        if (value.contains("path") && owner)
            target = owner->findEntityByPath(value.at("path").get<std::string>());
        else if (owner && owner->scene && value.contains("uid"))
            target = owner->scene->findEntityByUID(value.at("uid").get<UID>());
        else if (owner && owner->scene && value.contains("owner"))
            target = owner->scene->findEntityByUID(value.at("owner").get<UID>());
        return target && m_scriptInstance->setPropertyReference(name, target);
    }
    return false;
}

bool ScriptComponent::setPropertyReference(const std::string& name, Entity* target)
{
    return m_scriptInstance && m_scriptInstance->setPropertyReference(name, target);
}

bool ScriptComponent::clearPropertyReference(const std::string& name)
{
    return m_scriptInstance && m_scriptInstance->clearPropertyReference(name);
}

nlohmann::json ScriptComponent::getStateSnapshot() const
{
    nlohmann::json snapshot = nlohmann::json::object();
    for (const ScriptPropertyDescriptor& property : getScriptProperties())
    {
        nlohmann::json value = getPropertyValue(property.name);
        if (!value.is_null() || property.kind == ScriptPropertyKind::Reference)
            snapshot[property.name] = std::move(value);
    }
    return snapshot;
}

void ScriptComponent::saveStateSnapshot()
{
    m_StateSnapshot = getStateSnapshot();
}

void ScriptComponent::releaseScriptInstance()
{
    m_scriptInstance.reset();
    m_isInitialized = false;
}

void ScriptComponent::loadStateSnapshot()
{
    if (m_StateSnapshot.is_array())
    {
        for (const auto& snapshot : m_StateSnapshot)
            loadScriptStateFromValues(snapshot);
    }
    else
    {
        loadScriptStateFromValues(m_StateSnapshot);
    }
}

void ScriptComponent::loadScriptStateFromValues(const nlohmann::json& snapshot)
{
    if (!snapshot.is_object()) return;
    for (const auto& [name, value] : snapshot.items())
        setPropertyValue(name, value);
}

void ScriptComponent::save(nlohmann::json& j) const
{
    j["className"] = m_ClassName;
    j["fields"] = getStateSnapshot();
}

void ScriptComponent::load(const nlohmann::json& j)
{
    m_ClassName = j.at("className").get<std::string>();
    if (j.contains("fields"))
    {
        const nlohmann::json& fields = j.at("fields");
        if (!m_StateSnapshot.is_object()) m_StateSnapshot = nlohmann::json::object();
        if (fields.is_object()) m_StateSnapshot.update(fields);
    }

    if (!m_isInitialized)
        init(m_ClassName);
}
