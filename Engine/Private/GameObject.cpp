#include "GameObject.h"
#include "GameInstance.h"

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

    const char* basePtr = reinterpret_cast<const char*>(this);

    for (const auto& prop : info.properties)
    {
        const string key = prop.jsonKey.empty() ? prop.name : prop.jsonKey;
        const void* memberPtr = basePtr + prop.offset;

        switch (prop.type)
        {
        case PropertyType::Float:
            j["custom_properties"][key] = *static_cast<const float*>(memberPtr);
            break;

        case PropertyType::Int:
            j["custom_properties"][key] = *static_cast<const int*>(memberPtr);
            break;

        case PropertyType::Bool:
            j["custom_properties"][key] = *static_cast<const bool*>(memberPtr);
            break;

        case PropertyType::Vec3:
        {
            const Vec3* v = static_cast<const Vec3*>(memberPtr);
            j["custom_properties"][key] = { v->x, v->y, v->z };
            break;
        }

        case PropertyType::String:
            j["custom_properties"][key] = *static_cast<const string*>(memberPtr);
            break;

        default:
            break;
        }
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
    auto& info = Get_ReflectionInfo();

    if (data.contains("custom_properties") && data["custom_properties"].is_object())
    {
        const json& customProperties = data["custom_properties"];
        char* basePtr = reinterpret_cast<char*>(this);

        for (const auto& prop : info.properties)
        {
            const string key = prop.jsonKey.empty() ? prop.name : prop.jsonKey;
            if (!customProperties.contains(key))
                continue;

            void* memberPtr = basePtr + prop.offset;

            switch (prop.type)
            {
            case PropertyType::Float:
                *static_cast<float*>(memberPtr) = customProperties[key].get<float>();
                break;

            case PropertyType::Int:
                *static_cast<int*>(memberPtr) = customProperties[key].get<int>();
                break;

            case PropertyType::Bool:
                *static_cast<bool*>(memberPtr) = customProperties[key].get<bool>();
                break;

            case PropertyType::Vec3:
            {
                const json& arr = customProperties[key];
                if (!arr.is_array() || arr.size() < 3)
                    break;

                Vec3* v = static_cast<Vec3*>(memberPtr);
                v->x = arr[0].get<float>();
                v->y = arr[1].get<float>();
                v->z = arr[2].get<float>();
                break;
            }

            case PropertyType::String:
                *static_cast<string*>(memberPtr) = customProperties[key].get<string>();
                break;

            default:
                break;
            }
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
