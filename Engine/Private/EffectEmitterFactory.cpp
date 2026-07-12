#include "EffectEmitterFactory.h"

#include "EffectEmitter.h"

NS_BEGIN(Engine)

namespace Internal
{
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
