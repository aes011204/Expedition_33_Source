#include "TrailSampleProvider_ByPart.h"

#include "EffectCom.h"

NS_BEGIN(Client)

TrailSampleProvider_ByPart::TrailSampleProvider_ByPart(
    const Weak<EffectCom>& effectCom,
    wstring partTag,
    const TrailAnchorAsset& trailAnchorAsset)
    : _effectCom(effectCom)
    , _partTag(std::move(partTag))
    , _trailAnchorAsset(trailAnchorAsset)
{
}

bool TrailSampleProvider_ByPart::Try_GetTrailSample(EffectTrailSample& outSample) const
{
    const Shared<EffectCom> effectCom = _effectCom.lock();
    if (effectCom == nullptr)
        return false;

    return effectCom->Try_GetTrailSampleForPart(_partTag, _trailAnchorAsset, outSample);
}

NS_END
