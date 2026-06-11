#pragma once

#include "AnimNotify.h"

NS_BEGIN(Client)

class AN_PlaySound : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_PlaySound)

public:
    AN_PlaySound() = default;
    ~AN_PlaySound() override = default;

public:
    string Get_TypeName() const override { return "AN_PlaySound"; }
    void Execute(const AnimNotifyContext& context) override;
    void From_Json(const json& data) override;

private:
    wstring _soundCue = L"";
    float _volume = 1.f;
    float _pitch = 1.f;
};

NS_END
