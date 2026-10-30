#include "AngelScriptRuntime.h"

#include "Entity.h"
#include "ScriptComponent.h"
#include <cstdio>
#include <unordered_map>
#include <utility>

namespace
{
class AngelScriptInstance final : public IScriptInstance
{
public:
    AngelScriptInstance(asIScriptObject* object, asIScriptFunction* startMethod,
                        asIScriptFunction* updateMethod, std::string className)
        : m_object(object),
          m_startMethod(startMethod),
          m_updateMethod(updateMethod),
          m_className(std::move(className))
    {
        cacheProperties();
    }

    ~AngelScriptInstance() override
    {
        if (m_object) m_object->Release();
    }

    const std::string& getClassName() const override { return m_className; }
    const std::vector<ScriptPropertyDescriptor>& getProperties() const override { return m_properties; }
    asIScriptObject* getObject() const { return m_object; }
    void setOwner(Entity* owner) { m_owner = owner; }

    bool invokeStart() override
    {
        return invoke(m_startMethod, false, 0.0f);
    }

    bool invokeUpdate(float dt) override
    {
        return invoke(m_updateMethod, true, dt);
    }

    nlohmann::json getPropertyValue(const std::string& name) const override
    {
        const PropertyBinding* binding = findBinding(name);
        if (!binding || !binding->data) return nullptr;

        switch (binding->kind)
        {
            case ScriptPropertyKind::Float:
                return *static_cast<float*>(binding->data);
            case ScriptPropertyKind::Integer:
                return *static_cast<int*>(binding->data);
            case ScriptPropertyKind::Boolean:
                return *static_cast<bool*>(binding->data);
            case ScriptPropertyKind::String:
                return *static_cast<std::string*>(binding->data);
            default:
                return nullptr;
        }
    }

    bool setPropertyValue(const std::string& name, const nlohmann::json& value) override
    {
        const PropertyBinding* binding = findBinding(name);
        if (!binding || !binding->data) return false;

        if (binding->kind == ScriptPropertyKind::Float && value.is_number())
            *static_cast<float*>(binding->data) = value.get<float>();
        else if (binding->kind == ScriptPropertyKind::Integer && value.is_number_integer())
            *static_cast<int*>(binding->data) = value.get<int>();
        else if (binding->kind == ScriptPropertyKind::Boolean && value.is_boolean())
            *static_cast<bool*>(binding->data) = value.get<bool>();
        else if (binding->kind == ScriptPropertyKind::String && value.is_string())
            *static_cast<std::string*>(binding->data) = value.get<std::string>();
        else
            return false;
        return true;
    }

    Entity* getPropertyReference(const std::string& name) const override
    {
        const PropertyBinding* binding = findBinding(name);
        if (!binding || binding->kind != ScriptPropertyKind::Reference || !binding->data)
            return nullptr;

        void* target = *static_cast<void**>(binding->data);
        if (!target) return nullptr;
        if (binding->typeName == "Entity") return static_cast<Entity*>(target);

        if (binding->isScriptObject && m_owner && m_owner->scene)
        {
            for (const auto& entity : m_owner->scene->m_entities)
            {
                for (ScriptComponent* script : entity->getComponents<ScriptComponent>())
                {
                    auto* scriptInstance =
                        dynamic_cast<AngelScriptInstance*>(script->getScriptInstance());
                    if (scriptInstance && scriptInstance->getObject() == target)
                    {
                        return entity.get();
                    }
                }
            }
            return nullptr;
        }

        auto* component = static_cast<Component*>(target);
        return component ? component->owner : nullptr;
    }

    bool setPropertyReference(const std::string& name, Entity* target) override
    {
        const PropertyBinding* binding = findBinding(name);
        if (!binding || binding->kind != ScriptPropertyKind::Reference || !binding->data || !target)
            return false;

        void* reference = nullptr;
        if (binding->typeName == "Entity")
            reference = target;
        else if (binding->isScriptObject)
        {
            reference = AngelScriptRuntime::FindScriptObject(target, binding->typeName);
            if (!reference) return false;
            static_cast<asIScriptObject*>(reference)->AddRef();
        }
        else if (Component* component = target->getComponentByName(binding->typeName))
            reference = component;

        if (!reference) return false;
        void** handle = static_cast<void**>(binding->data);
        releaseReference(*handle, *binding);
        *handle = reference;
        return true;
    }

    bool clearPropertyReference(const std::string& name) override
    {
        const PropertyBinding* binding = findBinding(name);
        if (!binding || binding->kind != ScriptPropertyKind::Reference || !binding->data)
            return false;

        void** handle = static_cast<void**>(binding->data);
        releaseReference(*handle, *binding);
        *handle = nullptr;
        return true;
    }

private:
    struct PropertyBinding
    {
        int typeId = 0;
        void* data = nullptr;
        ScriptPropertyKind kind = ScriptPropertyKind::Unsupported;
        std::string typeName;
        bool isScriptObject = false;
    };

    const PropertyBinding* findBinding(const std::string& name) const
    {
        auto it = m_bindings.find(name);
        return it == m_bindings.end() ? nullptr : &it->second;
    }

    static void releaseReference(void* reference, const PropertyBinding& binding)
    {
        if (reference && binding.isScriptObject)
            static_cast<asIScriptObject*>(reference)->Release();
    }

    bool invoke(asIScriptFunction* method, bool hasFloatArgument, float value)
    {
        if (!m_object || !method) return false;
        asIScriptContext* context = GetAngelScriptRuntime().getContext();
        if (!context || context->Prepare(method) < 0) return false;
        context->SetObject(m_object);
        if (hasFloatArgument) context->SetArgFloat(0, value);
        return context->Execute() == asEXECUTION_FINISHED;
    }

    void cacheProperties()
    {
        if (!m_object) return;
        asITypeInfo* type = m_object->GetObjectType();
        asIScriptEngine* engine = m_object->GetEngine();
        if (!type || !engine) return;

        const int stringTypeId = engine->GetTypeIdByDecl("string");
        const int typeId = type->GetTypeId();
        for (asUINT i = 0; i < type->GetPropertyCount(); ++i)
        {
            const char* propertyName = nullptr;
            int propertyTypeId = 0;
            bool isPrivate = false;
            type->GetProperty(i, &propertyName, &propertyTypeId, &isPrivate);
            if (isPrivate || !propertyName) continue;

            ScriptPropertyDescriptor descriptor;
            descriptor.name = propertyName;
            PropertyBinding binding;
            binding.typeId = propertyTypeId;
            binding.data = m_object->GetAddressOfProperty(i);

            if (propertyTypeId == asTYPEID_FLOAT)
            {
                descriptor.kind = ScriptPropertyKind::Float;
                descriptor.typeName = "float";
            }
            else if (propertyTypeId == asTYPEID_INT32)
            {
                descriptor.kind = ScriptPropertyKind::Integer;
                descriptor.typeName = "int";
            }
            else if (propertyTypeId == asTYPEID_BOOL)
            {
                descriptor.kind = ScriptPropertyKind::Boolean;
                descriptor.typeName = "bool";
            }
            else if (propertyTypeId == stringTypeId)
            {
                descriptor.kind = ScriptPropertyKind::String;
                descriptor.typeName = "string";
            }
            else if (propertyTypeId & asTYPEID_OBJHANDLE)
            {
                descriptor.kind = ScriptPropertyKind::Reference;
                asITypeInfo* propertyType = engine->GetTypeInfoById(propertyTypeId);
                descriptor.typeName = propertyType ? propertyType->GetName() : "Unknown";
                binding.isScriptObject = propertyType &&
                    (propertyType->GetFlags() & asOBJ_SCRIPT_OBJECT);
            }

            binding.kind = descriptor.kind;
            binding.typeName = descriptor.typeName;
            for (const std::string& metadata :
                 GetAngelScriptRuntime().getPropertyMetadata(typeId, i))
            {
                if (metadata == "HideInInspector")
                    descriptor.hidden = true;
                else if (metadata.rfind("Header(", 0) == 0)
                {
                    const size_t firstQuote = metadata.find('"');
                    const size_t lastQuote = metadata.rfind('"');
                    if (firstQuote != std::string::npos && lastQuote > firstQuote)
                        descriptor.header = metadata.substr(firstQuote + 1, lastQuote - firstQuote - 1);
                }
                else if (metadata.rfind("Tooltip(", 0) == 0)
                {
                    const size_t open = metadata.find('(');
                    const size_t close = metadata.rfind(')');
                    if (open != std::string::npos && close > open)
                        descriptor.tooltip = metadata.substr(open + 1, close - open - 1);
                }
                else if (metadata.rfind("Range(", 0) == 0)
                {
                    descriptor.hasRange = std::sscanf(metadata.c_str(), "Range(%f, %f)",
                                                      &descriptor.rangeMin, &descriptor.rangeMax) == 2;
                }
            }
            m_properties.push_back(std::move(descriptor));
            m_bindings.emplace(propertyName, std::move(binding));
        }
    }

    asIScriptObject* m_object = nullptr;
    asIScriptFunction* m_startMethod = nullptr;
    asIScriptFunction* m_updateMethod = nullptr;
    std::string m_className;
    Entity* m_owner = nullptr;
    std::vector<ScriptPropertyDescriptor> m_properties;
    std::unordered_map<std::string, PropertyBinding> m_bindings;

    friend class AngelScriptRuntime;
};
}

std::unique_ptr<IScriptInstance> AngelScriptRuntime::createInstance(
    const std::string& className, Entity* owner, std::string& scriptPath)
{
    asIScriptEngine* engine = m_engine;
    if (!engine) return nullptr;
    asIScriptModule* module = engine->GetModule(m_activeModuleName.c_str(), asGM_ONLY_IF_EXISTS);
    if (!module) return nullptr;
    asITypeInfo* type = module->GetTypeInfoByName(className.c_str());
    if (!type) return nullptr;

    const std::string factoryDecl = className + "@ " + className + "()";
    asIScriptFunction* factory = type->GetFactoryByDecl(factoryDecl.c_str());
    asIScriptContext* context = m_context;
    if (!factory || !context || context->Prepare(factory) < 0 ||
        context->Execute() != asEXECUTION_FINISHED)
        return nullptr;

    asIScriptObject* object = *static_cast<asIScriptObject**>(context->GetAddressOfReturnValue());
    if (!object) return nullptr;
    object->AddRef();

    const char* sectionName = nullptr;
    factory->GetDeclaredAt(&sectionName, nullptr, nullptr);
    if (sectionName) scriptPath = sectionName;

    asIScriptFunction* initMethod = type->GetMethodByDecl(
        m_options.sceneApi ? "void _Init(Entity@)" : "void _Init()");
    if (initMethod && context->Prepare(initMethod) >= 0)
    {
        context->SetObject(object);
        if (m_options.sceneApi)
            context->SetArgAddress(0, owner);
        if (context->Execute() != asEXECUTION_FINISHED)
        {
            object->Release();
            return nullptr;
        }
    }

    auto instance = std::make_unique<AngelScriptInstance>(
        object, type->GetMethodByDecl("void start()"),
        type->GetMethodByDecl("void update(float)"), className);
    instance->setOwner(owner);
    return instance;
}

std::vector<std::string> AngelScriptRuntime::getAvailableScriptClasses() const
{
    std::vector<std::string> classes;
    asIScriptModule* module = m_engine
        ? m_engine->GetModule(m_activeModuleName.c_str(), asGM_ONLY_IF_EXISTS)
        : nullptr;
    if (!module) return classes;

    asITypeInfo* baseType = module->GetTypeInfoByName("EntityScript");
    if (!baseType) return classes;
    for (asUINT i = 0; i < module->GetObjectTypeCount(); ++i)
    {
        asITypeInfo* type = module->GetObjectTypeByIndex(i);
        if (type && (type->GetFlags() & asOBJ_SCRIPT_OBJECT) &&
            !(type->GetFlags() & asOBJ_ABSTRACT) && type->DerivesFrom(baseType))
            classes.emplace_back(type->GetName());
    }
    return classes;
}

CScriptHandle AngelScriptRuntime::GetScriptOnEntity(Entity* entity, const std::string& className)
{
    CScriptHandle handle;
    asIScriptObject* object = FindScriptObject(entity, className);
    if (object)
        handle.Set(object, object->GetObjectType());
    return handle;
}

asIScriptObject* AngelScriptRuntime::FindScriptObject(Entity* entity, const std::string& className)
{
    if (!entity) return nullptr;
    for (ScriptComponent* component : entity->getComponents<ScriptComponent>())
    {
        IScriptInstance* instance = component->getScriptInstance();
        auto* angelInstance = dynamic_cast<AngelScriptInstance*>(instance);
        if (angelInstance && angelInstance->getClassName() == className)
            return angelInstance->getObject();
    }
    return nullptr;
}

IScriptRuntime& GetScriptRuntime()
{
    return GetAngelScriptRuntime();
}

AngelScriptRuntime& GetAngelScriptRuntime()
{
    static AngelScriptRuntime runtime;
    return runtime;
}
