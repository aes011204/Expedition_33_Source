#pragma once

#include "EffectRuntime_Types.h"

NS_BEGIN(Engine)

class EffectEmitter;

// EffectInstance가 concrete emitter 구현을 직접 모르도록 생성 위임만 담당하는 엔진 공용 factory facade다.
class ENGINE_DLL EffectEmitterFactory final
{
public: //## Types::Public
    // app/client가 등록할 concrete emitter 생성 함수 시그니처다.
    using CreateEmitterFn = Shared<EffectEmitter>(*)(const EffectEmitterDefinition& emitterDefinition);

public: //## Behavior::Creation
    // 현재 runtime에서 사용할 concrete emitter 생성 함수를 등록한다.
    static void Register_CreateEmitterFn(CreateEmitterFn createEmitterFn);
    // EffectInstance가 emitter definition 하나를 concrete emitter로 생성 위임한다.
    static Shared<EffectEmitter> Create_EffectEmitter(const EffectEmitterDefinition& emitterDefinition);
};

NS_END
