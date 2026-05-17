#pragma once

#include "EffectRuntime_Types.h"

NS_BEGIN(Engine)

class EffectEmitter;

// EffectInstance가 concrete emitter 구현을 직접 모르도록 생성 위임만 담당하는 엔진 공용 factory facade다.
class ENGINE_DLL EffectEmitterFactory final
{
public:
    using CreateEmitterFn = Shared<EffectEmitter>(*)(const EffectEmitterDefinition& emitterDefinition); // app/client가 등록할 concrete emitter 생성 함수 시그니처다.

public:
    static void Register_CreateEmitterFn(CreateEmitterFn createEmitterFn); // 현재 runtime에서 사용할 concrete emitter 생성 함수를 등록한다.
    static Shared<EffectEmitter> Create_EffectEmitter(const EffectEmitterDefinition& emitterDefinition); // EffectInstance가 emitter definition 하나를 concrete emitter로 생성 위임한다.
};

NS_END
