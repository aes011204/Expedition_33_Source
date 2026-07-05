#pragma once

#include "AnimNotifyState.h"

NS_BEGIN(Client)

class EffectCom;

class ANS_WeakColor final : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_WeakColor)

public:
    ANS_WeakColor() = default;
    ~ANS_WeakColor() override = default;

public:
    string Get_TypeName() const override { return "ANS_WeakColor"; }

    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    Shared<EffectCom> Resolve_WeakEffectCom(const AnimNotifyContext& context) const;
    vector<uint32> Resolve_EmitterIds() const;
    float Resolve_BlendStrength(float progress) const;
    void Apply_Color(const AnimNotifyContext& context, float progress) const;
    void Clear_Color(const AnimNotifyContext& context) const;

private:
    wstring _partTag = L"Part_Weak";
    string _emitterIds = "83,70";
    Vec4 _targetColor = Vec4(1.f, 0.9f, 0.f, 1.f);
    float _strength = 1.f;
    float _fadeInRatio = 0.25f;
    float _fadeOutRatio = 0.25f;
};

NS_END
