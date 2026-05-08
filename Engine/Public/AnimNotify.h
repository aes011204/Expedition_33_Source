#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class ENGINE_DLL AnimNotify : public Base
{
public:
    AnimNotify() = default;
    virtual ~AnimNotify() = default;

public:
    virtual string Get_TypeName() const = 0;
    virtual void Execute(const AnimNotifyContext& context) = 0;

    virtual json To_Json() const;
    virtual void From_Json(const json& data);

    virtual ClassReflectionInfo Get_RelectionInfo()
    {
        static ClassReflectionInfo emptyInfo{};
        return emptyInfo;
    }
};

NS_END
