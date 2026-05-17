#include "EffectEmitter.h"

EffectEmitter::EffectEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

EffectEmitter::EffectEmitter(const EffectEmitter& prototype)
    : GameObject{ prototype }
    , _effectOwner{ prototype._effectOwner }
    , _emitterId{ prototype._emitterId }
    , _emitterName{ prototype._emitterName }
    , _localPosition{ prototype._localPosition }
    , _localRotationDegrees{ prototype._localRotationDegrees }
    , _useLocalSpace{ prototype._useLocalSpace }
    , _enabled{ prototype._enabled }
{
}

HRESULT EffectEmitter::Attach_ToEffect(const EffectEmitterRuntimeDesc& runtimeDesc)
{
    _effectOwner = runtimeDesc.effectOwner;
    _emitterId = runtimeDesc.emitterId;
    _emitterName = runtimeDesc.emitterName;
    _localPosition = runtimeDesc.localPosition;
    _localRotationDegrees = runtimeDesc.localRotationDegrees;
    _useLocalSpace = runtimeDesc.useLocalSpace;
    _enabled = runtimeDesc.enabled;

    if (runtimeDesc.effectOwner)
        Set_Owner(runtimeDesc.effectOwner);

    Sync_FromEffectOwner();

    return S_OK;
}

void EffectEmitter::Sync_FromEffectOwner()
{
    CHECK_NULL_THROTTLED(_transformCom, 120);

    const Shared<GameObject> effectOwner = _effectOwner.lock();
    const Shared<TransformCom> ownerTransform =
        (effectOwner != nullptr) ? effectOwner->Get_Transform() : Shared<TransformCom>{};

    if (_useLocalSpace)
    {
        _transformCom->Set_Parent(ownerTransform);
        _transformCom->Set_Position(_localPosition);
        _transformCom->Set_RotationEuler(_localRotationDegrees);
    }
    else
    {
        _transformCom->Set_Parent(nullptr);
        _transformCom->Set_WorldPosition(_localPosition);
        _transformCom->Set_WorldRotationEuler(_localRotationDegrees);
    }

    On_EffectTransformSynced();
}
