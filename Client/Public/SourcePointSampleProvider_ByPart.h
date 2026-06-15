#pragma once

#include "Client_Defines.h"
#include "EffectRuntime_Types.h"
#include "TrailAnchor_Asset.h"

NS_BEGIN(Client)

class EffectCom;

enum class SourceHistorySourcePointPolicy : uint8
{
    Tip,
    Base,
    Mid,
};

enum class SourceHistorySourceBindingMode : uint8
{
    TrailAnchor,
    Socket,
};

class SourcePointSampleProvider_ByPart final : public IEffectSourcePointSampleProvider
{
public:
    SourcePointSampleProvider_ByPart(
        const Weak<EffectCom>& effectCom,
        wstring partTag,
        SourceHistorySourceBindingMode sourceBindingMode,
        string socketName,
        const TrailAnchorAsset& trailAnchorAsset,
        SourceHistorySourcePointPolicy sourcePointPolicy,
        Shared<bool> sampleEnabled = nullptr);

    ~SourcePointSampleProvider_ByPart() override = default;

public:
    bool Try_GetSourcePointSample(EffectSourcePointSample& outSample) const override;

private:
    Weak<EffectCom> _effectCom{};
    wstring _partTag{};
    SourceHistorySourceBindingMode _sourceBindingMode{ SourceHistorySourceBindingMode::TrailAnchor };
    string _socketName{};
    TrailAnchorAsset _trailAnchorAsset{};
    SourceHistorySourcePointPolicy _sourcePointPolicy{ SourceHistorySourcePointPolicy::Tip };
    Shared<bool> _sampleEnabled{};
};

NS_END
