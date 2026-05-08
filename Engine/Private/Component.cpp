#include "Component.h"
#include "GameInstance.h"
#include "Reflection_JsonUtil.h"

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
        const void* memberPtr = basePtr + prop.offset;
        Reflection::Write_ReflectionProperty(root, prop, memberPtr);
    }

    return root;
}

void Component::From_Json(const json& data)
{
    auto& info = Get_ReflectionInfo();
    char* basePtr = reinterpret_cast<char*>(this);

    for (const auto& prop : info.properties)
    {
        void* memberPtr = basePtr + prop.offset;
        Reflection::Read_ReflectionProperty(data, prop, memberPtr);
    }
}

void Component::Free()
{
    __super::Free();
}
