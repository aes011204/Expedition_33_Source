#include "SourcePointSampleProvider_ByPart.h"

#include "EffectCom.h"

NS_BEGIN(Client)

SourcePointSampleProvider_ByPart::SourcePointSampleProvider_ByPart(
    const Weak<EffectCom>& effectCom,
    wstring partTag,
    SourceHistorySourceBindingMode sourceBindingMode,
    string socketName,
    const TrailAnchorAsset& trailAnchorAsset,
    SourceHistorySourcePointPolicy sourcePointPolicy,
    Shared<bool> sampleEnabled)
    : _effectCom(effectCom)
    , _partTag(move(partTag))
    , _sourceBindingMode(sourceBindingMode)
    , _socketName(move(socketName))
    , _trailAnchorAsset(trailAnchorAsset)
    , _sourcePointPolicy(sourcePointPolicy)
    , _sampleEnabled(move(sampleEnabled))
{
}

bool SourcePointSampleProvider_ByPart::Try_GetSourcePointSample(EffectSourcePointSample& outSample) const
{
    if (_sampleEnabled != nullptr && !*_sampleEnabled)
        return false;

    const Shared<EffectCom> effectCom = _effectCom.lock();
    if (effectCom == nullptr)
        return false;

    return effectCom->Try_GetSourcePointSampleForPart(
        _partTag,
        _sourceBindingMode,
        _socketName,
        _trailAnchorAsset,
        _sourcePointPolicy,
        outSample
    );
}

NS_END
