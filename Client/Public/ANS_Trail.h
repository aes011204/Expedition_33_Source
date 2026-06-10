#pragma once

#include "AnimNotifyState.h"

NS_BEGIN(Client)

class ANS_Trail : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_Trail)

public:
    ANS_Trail() = default;
    ~ANS_Trail() override = default;

public:
    string Get_TypeName() const override { return "ANS_Trail"; }

    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    wstring _effectName = L""; // 비어 있으면 Weapon 프리펩 기본 Trail 사용
    wstring _partTag = L"Part_Weapon";
    wstring _layerTag = L"Layer_Effect";
};

NS_END
