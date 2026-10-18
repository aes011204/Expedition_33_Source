#include "EffectEmitterFactory.h"

#include "EffectEmitter.h"

NS_BEGIN(Engine)

namespace Internal
{
    // 현재 process에서 effect emitter 생성을 실제로 수행할 등록 함수다.
    EffectEmitterFactory::CreateEmitterFn createEmitterFn = nullptr;
}

void EffectEmitterFactory::Register_CreateEmitterFn(CreateEmitterFn createEmitterFn)
{
    Internal::createEmitterFn = createEmitterFn;
}

Shared<EffectEmitter> EffectEmitterFactory::Create_EffectEmitter(const EffectEmitterDefinition& emitterDefinition)
{
    if (nullptr == Internal::createEmitterFn)
    {
        LOG_ERROR("EffectEmitterFactory create hook is not registered.");
        return nullptr;
    }

    return Internal::createEmitterFn(emitterDefinition);
}

NS_END
