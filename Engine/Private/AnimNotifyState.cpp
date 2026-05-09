#include "pch.h"
#include "AnimNotifyState.h"
#include "Reflection_JsonUtil.h"

json AnimNotifyState::To_Json() const
{
    json root = json::object();

    auto& info = const_cast<AnimNotifyState*>(this)->Get_ReflectionInfo();
    const char* basePtr = reinterpret_cast<const char*>(this);

    for (const auto& prop : info.properties)
    {
        const void* memberPtr = basePtr + prop.offset;
        Reflection::Write_ReflectionProperty(root, prop, memberPtr);
    }

    return root;
}

void AnimNotifyState::From_Json(const json& data)
{
    auto& info = Get_ReflectionInfo();
    char* basePtr = reinterpret_cast<char*>(this);

    for (const auto& prop : info.properties)
    {
        void* memberPtr = basePtr + prop.offset;
        Reflection::Read_ReflectionProperty(data, prop, memberPtr);
    }
}
