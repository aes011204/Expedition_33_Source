#pragma once

#include "Base.h"
#include "AnimNotify_Types.h"

NS_BEGIN(Engine)

class ENGINE_DLL AnimNotifyState : public Base
{
public:
    AnimNotifyState() = default;
    virtual ~AnimNotifyState() = default;

public:
    virtual string Get_TypeName() const = 0;

    virtual void On_Begin(const AnimNotifyContext& context) = 0;
    virtual void On_Tick(const AnimNotifyContext& context) = 0;
    virtual void On_End(const AnimNotifyContext& context) = 0;

    virtual json To_Json() const;
    virtual void From_Json(const json& data);

    virtual ClassReflectionInfo& Get_ReflectionInfo()
    {
        static ClassReflectionInfo emptyInfo{};
        return emptyInfo;
    }
};

NS_END
