#pragma once

#include "AnimNotifyState.h"

NS_BEGIN(Client)

class Character_BattleBase;

class ANS_Visible_Parts : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_Visible_Parts)

public:
    ANS_Visible_Parts() = default;
    virtual ~ANS_Visible_Parts() = default;

public:
    string Get_TypeName() const override { return "ANS_Visible_Parts"; }
    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    float _start = 1.f;
    float _end = 1.f;
    wstring  _string =L"";

    Character_BattleBase* chart = {};
};

NS_END
