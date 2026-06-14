#pragma once
#include "EffectRuntime_Types.h"

NS_BEGIN(Client)

// .effect.json authoring source를 Client runtime EffectDefinition으로 접는 loader다.
class EffectAssetRuntimeLoader
{
public:
    EffectAssetRuntimeLoader() = delete;

public: //## Entry Points
    // 리소스 루트 기준 .effect.json 경로를 EffectDefinition으로 변환한다.
    static Shared<const EffectDefinition> Load_Definition(const wstring& effectAssetPath);
    // Effects/Assets 아래의 .effect.json 파일 이름으로 EffectDefinition을 로드한다.
    static Shared<const EffectDefinition> Load_DefinitionByName(const wstring& effectName);
};

NS_END
