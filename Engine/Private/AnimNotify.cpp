#include "AnimNotify.h"
#include "Reflection_JsonUtil.h"

json AnimNotify::To_Json() const
{
    json root = json::object();

    const auto& info = const_cast<AnimNotify*>(this)->Get_ReflectionInfo();
    const char* basePtr = reinterpret_cast<const char*>(this);

    for (const auto& prop : info.properties)
    {
        const void* memberPtr = basePtr + prop.offset;
        Reflection::Write_ReflectionProperty(root, prop, memberPtr);
    }

    return root;
}

void AnimNotify::From_Json(const json& data)
{
    const auto& info = Get_ReflectionInfo();
    char* basePtr = reinterpret_cast<char*>(this);

    for (const auto& prop : info.properties)
    {
        void* memberPtr = basePtr + prop.offset;
        Reflection::Read_ReflectionProperty(data, prop, memberPtr);
    }
}
