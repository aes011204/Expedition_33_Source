#pragma once

#include "AnimNotifyState.h"

NS_BEGIN(Client)

class Character_BattleBase;

class ANS_Emissive : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_Emissive)

public:
    ANS_Emissive() = default;
    virtual ~ANS_Emissive() = default;

public:
    string Get_TypeName() const override { return "ANS_Emissive"; }
    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    float _start = 1.f;
    float _end = 1.f;
    wstring  _partName = L"";
    int   _matIndex = {};

    ModelCom* modelCom = {};


};

NS_END
