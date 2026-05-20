#pragma once

#include "Client_Defines.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(Client)

// .effect.json authoring source를 Client runtime EffectDefinition으로 접는 loader다.
class EffectAssetRuntimeLoader
{
public:
    EffectAssetRuntimeLoader() = delete;

public:
    static Shared<const EffectDefinition> Load_Definition(const wstring& effectAssetPath);
};

NS_END
