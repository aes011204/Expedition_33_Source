#include "Create_Class.h"

#include "AnimNotify.h"
#include "AnimNotifyState.h"
#include "BNode.h"
#include "Component.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "Reflection_JsonUtil.h"
#include "TransformCom.h"
#include "SkillScript.h"

namespace
{
    void Apply_ReflectionProperties(void* instance, Engine::ClassReflectionInfo& info, const json& properties)
    {
        if (instance == nullptr || properties.is_null() || !properties.is_object())
            return;

        char* base = reinterpret_cast<char*>(instance);

        for (const Engine::PropertyInfo& property : info.properties)
        {
            const string key = property.jsonKey.empty() ? property.name : property.jsonKey;
            if (!properties.contains(key))
                continue;

            void* address = base + property.offset;
            json wrapper = json::object();
            wrapper[key] = properties[key];
            Engine::Reflection::Read_ReflectionProperty(wrapper, property, address);
        }
    }
}

Create_Class::ActorBuilder::ActorBuilder(const string& staticClass)
{
    _desc.staticClass = staticClass;
}

Create_Class::ActorBuilder& Create_Class::ActorBuilder::AtLevel(uint32 levelIndex)
{
    _desc.levelIndex = levelIndex;
    return *this;
}

Create_Class::ActorBuilder& Create_Class::ActorBuilder::InLayer(const wstring& layerTag)
{
    _desc.layerTag = layerTag;
    return *this;
}

Create_Class::ActorBuilder& Create_Class::ActorBuilder::InitializeArg(void* arg)
{
    _desc.initializeArg = arg;
    return *this;
}

Create_Class::ActorBuilder& Create_Class::ActorBuilder::Position(const Vec3& position)
{
    _desc.transformOverrides["position"] = { position.x, position.y, position.z };
    return *this;
}

Create_Class::ActorBuilder& Create_Class::ActorBuilder::Rotation(const Vec3& eulerDegree)
{
    _desc.transformOverrides["rotation"] = { eulerDegree.x, eulerDegree.y, eulerDegree.z };
    return *this;
}

Create_Class::ActorBuilder& Create_Class::ActorBuilder::Scale(const Vec3& scale)
{
    _desc.transformOverrides["scale"] = { scale.x, scale.y, scale.z };
    return *this;
}

Create_Class::ActorBuilder& Create_Class::ActorBuilder::Property(const string& propertyName, const json& value)
{
    if (!propertyName.empty())
        _desc.properties[propertyName] = value;

    return *this;
}

Shared<Engine::GameObject> Create_Class::ActorBuilder::Create() const
{
    return Create_Class::Spawn_ActorImplementation(_desc);
}

Shared<Engine::GameObject> Create_Class::ActorBuilder::Spawn() const
{
    return Create();
}

Create_Class::ComponentBuilder::ComponentBuilder(const string& staticClass)
{
    _desc.staticClass = staticClass;
}

Create_Class::ComponentBuilder& Create_Class::ComponentBuilder::InitializeArg(void* arg)
{
    _desc.initializeArg = arg;
    return *this;
}

Create_Class::ComponentBuilder& Create_Class::ComponentBuilder::Property(const string& propertyName, const json& value)
{
    if (!propertyName.empty())
        _desc.properties[propertyName] = value;

    return *this;
}

Shared<Engine::Component> Create_Class::ComponentBuilder::Create() const
{
    return Create_Class::Create_ComponentImplementation(_desc);
}

Create_Class::BTNodeBuilder::BTNodeBuilder(const string& staticClass)
{
    _desc.staticClass = staticClass;
}

Create_Class::BTNodeBuilder& Create_Class::BTNodeBuilder::Property(const string& propertyName, const json& value)
{
    if (!propertyName.empty())
        _desc.properties[propertyName] = value;

    return *this;
}

Shared<Engine::BNode> Create_Class::BTNodeBuilder::Create() const
{
    return Create_Class::Create_BTNodeImplementation(_desc);
}

Create_Class::NotifyBuilder::NotifyBuilder(const string& staticClass)
{
    _desc.staticClass = staticClass;
}

Create_Class::NotifyBuilder& Create_Class::NotifyBuilder::Property(const string& propertyName, const json& value)
{
    if (!propertyName.empty())
        _desc.properties[propertyName] = value;

    return *this;
}

Shared<Engine::AnimNotify> Create_Class::NotifyBuilder::Create() const
{
    return Create_Class::Create_NotifyImplementation(_desc);
}

Create_Class::NotifyStateBuilder::NotifyStateBuilder(const string& staticClass)
{
    _desc.staticClass = staticClass;
}

Create_Class::NotifyStateBuilder& Create_Class::NotifyStateBuilder::Property(const string& propertyName, const json& value)
{
    if (!propertyName.empty())
        _desc.properties[propertyName] = value;

    return *this;
}

Shared<Engine::AnimNotifyState> Create_Class::NotifyStateBuilder::Create() const
{
    return Create_Class::Create_NotifyStateImplementation(_desc);
}

Create_Class::SkillScriptBuilder::SkillScriptBuilder(const string& staticClass)
{
    _desc.staticClass = staticClass;
}

Shared<SkillScript> Create_Class::SkillScriptBuilder::Create() const
{
    if (_desc.staticClass.empty())
        return nullptr;

    return Client::SkillScript::Create_SkillScriptByClass(_desc.staticClass);
}

Shared<Engine::GameObject> Create_Class::SpawnActor(
    const string& staticClass,
    uint32 levelIndex,
    const wstring& layerTag)
{
    ActorDesc desc{};
    desc.staticClass = staticClass;
    desc.levelIndex = levelIndex;
    desc.layerTag = layerTag;

    return Spawn_ActorImplementation(desc);
}

Shared<Engine::GameObject> Create_Class::SpawnActorAt(
    const string& staticClass,
    uint32 levelIndex,
    const wstring& layerTag,
    const Vec3& position)
{
    ActorDesc desc{};
    desc.staticClass = staticClass;
    desc.levelIndex = levelIndex;
    desc.layerTag = layerTag;
    desc.transformOverrides["position"] = { position.x, position.y, position.z };

    return Spawn_ActorImplementation(desc);
}

void Create_Class::Apply_TransformOverrides(
    const Shared<GameObject>& gameObject,
    const json& overrides)
{
    if (gameObject == nullptr || overrides.is_null() || !overrides.is_object())
        return;

    Shared<TransformCom> transform = gameObject->Get_Transform();
    if (transform == nullptr)
        return;

    if (overrides.contains("position"))
    {
        const json& arr = overrides["position"];
        if (arr.is_array() && arr.size() >= 3)
        {
            transform->Set_WorldPosition(Vec3(
                arr[0].get<float>(),
                arr[1].get<float>(),
                arr[2].get<float>()));
        }
    }

    if (overrides.contains("rotation"))
    {
        const json& arr = overrides["rotation"];
        if (arr.is_array() && arr.size() >= 3)
        {
            transform->Set_WorldRotationEuler(Vec3(
                arr[0].get<float>(),
                arr[1].get<float>(),
                arr[2].get<float>()));
        }
    }

    if (overrides.contains("scale"))
    {
        const json& arr = overrides["scale"];
        if (arr.is_array() && arr.size() >= 3)
        {
            transform->Set_Scale(Vec3(
                arr[0].get<float>(),
                arr[1].get<float>(),
                arr[2].get<float>()));
        }
    }
}

Shared<Engine::GameObject> Create_Class::Spawn_ActorImplementation(const ActorDesc& desc)
{
    if (desc.staticClass.empty())
        return nullptr;

    Shared<Engine::GameObject> gameObject = GAME->Create_GameObjectPrototype(desc.staticClass);
    if (gameObject == nullptr)
        return nullptr;

    if (FAILED(gameObject->Initialize(desc.initializeArg)))
        return nullptr;

    auto& info = gameObject->Get_ReflectionInfo();
    Apply_ReflectionProperties(gameObject.get(), info, desc.properties);
    Apply_TransformOverrides(gameObject, desc.transformOverrides);

    if (FAILED(GAME->Add_GameObject(desc.levelIndex, desc.layerTag, gameObject)))
        return nullptr;

    return gameObject;
}

Shared<Engine::Component> Create_Class::Create_ComponentImplementation(const InstanceDesc& desc)
{
    if (desc.staticClass.empty())
        return nullptr;

    Shared<Engine::Component> component = GAME->Create_ComponentPrototype(desc.staticClass);
    if (component == nullptr)
        return nullptr;

    if (FAILED(component->Initialize(desc.initializeArg)))
        return nullptr;

    auto& info = component->Get_ReflectionInfo();
    Apply_ReflectionProperties(component.get(), info, desc.properties);

    return component;
}

Shared<Engine::BNode> Create_Class::Create_BTNodeImplementation(const InstanceDesc& desc)
{
    if (desc.staticClass.empty())
        return nullptr;

    Shared<Engine::BNode> node = dynamic_pointer_cast<Engine::BNode>(GAME->Create_BTNodePrototype(desc.staticClass));
    if (node == nullptr)
        return nullptr;

    auto& info = node->Get_ReflectionInfo();
    Apply_ReflectionProperties(node.get(), info, desc.properties);

    return node;
}

Shared<Engine::AnimNotify> Create_Class::Create_NotifyImplementation(const InstanceDesc& desc)
{
    if (desc.staticClass.empty())
        return nullptr;

    Shared<Engine::AnimNotify> notify = GAME->Create_AnimNotifyPrototype(desc.staticClass);
    if (notify == nullptr)
        return nullptr;

    auto& info = notify->Get_ReflectionInfo();
    Apply_ReflectionProperties(notify.get(), info, desc.properties);

    return notify;
}

Shared<Engine::AnimNotifyState> Create_Class::Create_NotifyStateImplementation(const InstanceDesc& desc)
{
    if (desc.staticClass.empty())
        return nullptr;

    Shared<Engine::AnimNotifyState> notifyState = GAME->Create_AnimNotifyStatePrototype(desc.staticClass);
    if (notifyState == nullptr)
        return nullptr;

    auto& info = notifyState->Get_ReflectionInfo();
    Apply_ReflectionProperties(notifyState.get(), info, desc.properties);

    return notifyState;
}

