#include "EffectInstance.h"

IMPLEMENT_REFLECTION(EffectInstance)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "EffectInstance";
    info.category = "Effect";
    return true;
}

EffectInstance::EffectInstance(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

EffectInstance::EffectInstance(const EffectInstance& prototype)
    : GameObject{ prototype }
{
}

EffectInstance::~EffectInstance()
{
    Free();
}

HRESULT EffectInstance::Initialize_Prototype()
{
    return S_OK;
}

HRESULT EffectInstance::Initialize(void* arg)
{
    if (nullptr != arg)
        _desc = *static_cast<EffectInstanceDesc*>(arg);

    _definition = _desc.definition;

    CHECK_FAILED(__super::Initialize(&_desc), E_FAIL);
    CHECK_FAILED(Ready_Emitters(), E_FAIL);

    return S_OK;
}

void EffectInstance::BeginPlay()
{
    __super::BeginPlay();

    for (const auto& emitter : _emitters)
    {
        if (emitter)
            (void)emitter->Try_BeginPlay();
    }
}

void EffectInstance::Priority_Update(float timeDelta)
{
    Sync_Emitters();

    for (const auto& emitter : _emitters)
    {
        if (emitter && emitter->Is_EmitterEnabled())
            emitter->Priority_Update(timeDelta);
    }
}

void EffectInstance::Update(float timeDelta)
{
    Sync_Emitters();

    for (const auto& emitter : _emitters)
    {
        if (emitter && emitter->Is_EmitterEnabled())
            emitter->Update(timeDelta);
    }
}

void EffectInstance::Late_Update(float timeDelta)
{
    Sync_Emitters();

    for (const auto& emitter : _emitters)
    {
        if (emitter && emitter->Is_EmitterEnabled())
            emitter->Late_Update(timeDelta);
    }
}

HRESULT EffectInstance::Render()
{
    return S_OK;
}

HRESULT EffectInstance::Ready_Emitters()
{
    Clear_Emitters();

    if (nullptr == _definition)
        return E_FAIL;

    const Shared<GameObject> rootOwner = GetSharedPtr<GameObject>();

    for (const EffectEmitterDefinition& emitterDefinition : _definition->emitters)
    {
        if (!emitterDefinition.enabled)
            continue;

        Shared<EffectEmitter> emitter = EffectEmitterFactory::Create_EffectEmitter(emitterDefinition);
        CHECK_NULL(emitter, E_FAIL);

        EffectEmitter::EffectEmitterRuntimeDesc runtimeDesc{};
        runtimeDesc.effectOwner = rootOwner;
        runtimeDesc.emitterId = emitterDefinition.id;
        runtimeDesc.emitterName = emitterDefinition.name;
        runtimeDesc.localPosition = emitterDefinition.localPosition;
        runtimeDesc.localRotationDegrees = emitterDefinition.localRotationDegrees;
        runtimeDesc.useLocalSpace = emitterDefinition.useLocalSpace;
        runtimeDesc.enabled = emitterDefinition.enabled;

        CHECK_FAILED(emitter->Attach_ToEffect(runtimeDesc), E_FAIL);
        _emitters.push_back(emitter);
    }

    return S_OK;
}

void EffectInstance::Sync_Emitters()
{
    for (const Shared<EffectEmitter>& emitter : _emitters)
    {
        if (nullptr == emitter)
            continue;

        emitter->Set_Visible(Is_Visible());
        emitter->Sync_FromEffectOwner();
    }
}

void EffectInstance::Clear_Emitters()
{
    _emitters.clear();
}

Shared<EffectInstance> EffectInstance::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<EffectInstance>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : EffectInstance");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> EffectInstance::Clone(void* arg)
{
    auto instance = make_shared<EffectInstance>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : EffectInstance");
        MSG_BOX("Failed to Clone : EffectInstance");
        return nullptr;
    }

    return instance;
}

void EffectInstance::Free()
{
    Clear_Emitters();

    __super::Free();
}
