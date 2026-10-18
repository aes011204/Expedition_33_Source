#pragma once

#include "AnimNotify.h"

NS_BEGIN(Client)

class AN_WeaponDetach final : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_WeaponDetach)

public:
    AN_WeaponDetach() = default;
    ~AN_WeaponDetach() override = default;

public:
    string Get_TypeName() const override { return "AN_WeaponDetach"; }
    void Execute(const AnimNotifyContext& context) override;

private:
    wstring _partTag = L"Part_Weapon";
    bool _applyScale = false;
    Vec3 _scale = Vec3::One;
};

NS_END
