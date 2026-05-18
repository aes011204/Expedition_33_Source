#include "GameObject.h"
#include "GameInstance.h"
#include "Reflection_JsonUtil.h"

GameObject::GameObject(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : _device { device }
    , _context { context }
{
}

GameObject::GameObject(const GameObject& prototype)
    : _device{ prototype._device }
    , _context{ prototype._context }
    , _hasBegunPlay(false)
{
}

HRESULT GameObject::Initialize_Prototype()
{
    return S_OK;
}

HRESULT GameObject::Initialize(void* arg)
{
    _transformCom = TransformCom::Create(_device, _context);

    if (nullptr == _transformCom)
        return E_FAIL;

    if (FAILED(_transformCom->Initialize(arg)))
        return E_FAIL;

    _transformCom->Set_Owner(GetSharedPtr<GameObject>());

    wstring transformTag = TEXT("Com_Transform");

    if (_components.end() == _components.find(transformTag))
        _components.emplace(transformTag, static_pointer_cast<Component>(_transformCom));

    return S_OK;
}

void GameObject::BeginPlay()
{
    for (const auto& [tag, component] : _components)
    {
        if (component == nullptr)
            continue;

        component->BeginPlay();
    }
}

void GameObject::Priority_Update(float timeDelta)
{
}

void GameObject::Update(float timeDelta)
{
}

void GameObject::Late_Update(float timeDelta)
{
}

HRESULT GameObject::Render()
{
    return S_OK;
}

HRESULT GameObject::Try_BeginPlay()
{
    if (_hasBegunPlay)
        return S_OK;

    BeginPlay();
    _hasBegunPlay = true;

    return S_OK;
}

json GameObject::To_Json() const
{
    json j = json::object();

    auto& info = const_cast<GameObject*>(this)->Get_ReflectionInfo();
    j["static_class"] = info.className;
    j["custom_properties"] = json::object();

    if (!_sourcePrefabName.empty())
        j["source_prefab"] = _sourcePrefabName;

    const char* basePtr = reinterpret_cast<const char*>(this);

    for (const auto& prop : info.properties)
    {
        const void* memberPtr = basePtr + prop.offset;
        Reflection::Write_ReflectionProperty(j["custom_properties"], prop, memberPtr);
    }

    json components = json::array();
    for (const auto& [tag, comp] : _components)
    {
        if (comp == nullptr)
            continue;

        json compJson = comp->To_Json();
        compJson["prototype_tag"] = String::ToString(tag);
        components.emplace_back(compJson);
    }

    j["components"] = components;

    return j;
}

void GameObject::From_Json(const json& data)
{
    Set_SourcePrefabName(data.value("source_prefab", string{}));

    auto& info = Get_ReflectionInfo();

    if (data.contains("custom_properties") && data["custom_properties"].is_object())
    {
        const json& customProperties = data["custom_properties"];
        char* basePtr = reinterpret_cast<char*>(this);

        for (const auto& prop : info.properties)
        {
            void* memberPtr = basePtr + prop.offset;
            Reflection::Read_ReflectionProperty(customProperties, prop, memberPtr);
        }
    }

    if (!data.contains("components") || !data["components"].is_array())
        return;

    for (const json& componentJson : data["components"])
    {
        if (!componentJson.is_object())
            continue;

        if (!componentJson.contains("prototype_tag"))
            continue;

        const string prototypeTagStr = componentJson["prototype_tag"].get<string>();
        const wstring prototypeTag = String::ToWString(prototypeTagStr);

        auto iter = _components.find(prototypeTag);
        if (iter == _components.end())
            continue;

        if (iter->second == nullptr)
            continue;

        iter->second->From_Json(componentJson);
    }
}


void GameObject::Free()
{
    __super::Free();

    _components.clear();
    _transformCom.reset();
}
