#pragma once

#include "AnimNotify.h"

NS_BEGIN(Client)

class AN_WeaponAttach final : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_WeaponAttach)

public:
    AN_WeaponAttach() = default;
    ~AN_WeaponAttach() override = default;

public:
    string Get_TypeName() const override { return "AN_WeaponAttach"; }
    void Execute(const AnimNotifyContext& context) override;

private:
    wstring _partTag = L"Part_Weapon";
    bool _applyScale = false;
    Vec3 _scale = Vec3::One;
};

NS_END
