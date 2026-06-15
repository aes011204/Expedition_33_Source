#pragma once

#include "AnimNotifyState.h"
#include "SourcePointSampleProvider_ByPart.h"

NS_BEGIN(Client)

class ANS_SourceHistoryEffect : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_SourceHistoryEffect)

public:
    ANS_SourceHistoryEffect() = default;
    ~ANS_SourceHistoryEffect() override = default;

public:
    string Get_TypeName() const override { return "ANS_SourceHistoryEffect"; }

    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    wstring _effectName = L"";
    wstring _partTag = L"Part_Weapon";
    wstring _layerTag = L"Layer_Effect";
    SourceHistorySourceBindingMode _sourceBindingMode{ SourceHistorySourceBindingMode::TrailAnchor };
    string _socketName{};
    SourceHistorySourcePointPolicy _sourcePointPolicy{ SourceHistorySourcePointPolicy::Tip };
};

NS_END
