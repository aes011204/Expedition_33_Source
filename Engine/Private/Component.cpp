#include "Component.h"
#include "GameInstance.h"

Component::Component(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : _device{ device }
    , _context{ context }
{
}

Component::Component(const Component& prototype)
    : _device{ prototype._device }
    , _context{ prototype._context }
{
}

HRESULT Component::Initialize_Prototype()
{
    return S_OK;
}

HRESULT Component::Initialize(void* arg)
{
    return S_OK;
}

void Component::BeginPlay()
{

}

json Component::To_Json() const
{
    json root = json::object();

    auto& info = const_cast<Component*>(this)->Get_ReflectionInfo();
    root["class_name"] = info.className;

    const char* basePtr = reinterpret_cast<const char*>(this);

    for (const auto& prop : info.properties)
    {
        const string key = prop.jsonKey.empty() ? prop.name : prop.jsonKey;
        const void* memberPtr = basePtr + prop.offset;

        switch (prop.type)
        {
        case PropertyType::Float:
            root[key] = *static_cast<const float*>(memberPtr);
            break;

        case PropertyType::Int:
            root[key] = *static_cast<const int*>(memberPtr);
            break;

        case PropertyType::Bool:
            root[key] = *static_cast<const bool*>(memberPtr);
            break;

        case PropertyType::Vec3:
        {
            const Vec3* v = static_cast<const Vec3*>(memberPtr);
            root[key] = { v->x, v->y, v->z };
            break;
        }

        case PropertyType::String:
            root[key] = *static_cast<const string*>(memberPtr);
            break;

        default:
            break;
        }
    }

    return root;
}

void Component::From_Json(const json& data)
{
    auto& info = Get_ReflectionInfo();
    char* basePtr = reinterpret_cast<char*>(this);

    for (const auto& prop : info.properties)
    {
        const string key = prop.jsonKey.empty() ? prop.name : prop.jsonKey;
        if (!data.contains(key))
            continue;

        void* memberPtr = basePtr + prop.offset;

        switch (prop.type)
        {
        case PropertyType::Float:
            *static_cast<float*>(memberPtr) = data[key].get<float>();
            break;

        case PropertyType::Int:
            *static_cast<int*>(memberPtr) = data[key].get<int>();
            break;

        case PropertyType::Bool:
            *static_cast<bool*>(memberPtr) = data[key].get<bool>();
            break;

        case PropertyType::Vec3:
        {
            const json& arr = data[key];
            if (!arr.is_array() || arr.size() < 3)
                break;

            Vec3* v = static_cast<Vec3*>(memberPtr);
            v->x = arr[0].get<float>();
            v->y = arr[1].get<float>();
            v->z = arr[2].get<float>();
            break;
        }

        case PropertyType::String:
            *static_cast<string*>(memberPtr) = data[key].get<string>();
            break;

        default:
            break;
        }
    }
}

void Component::Free()
{
    __super::Free();
}
