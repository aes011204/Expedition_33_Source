#pragma once

#include "Player_BattleBase.h"

NS_BEGIN(Client)

class PB_Lune final : public Player_BattleBase
{
    GENERATED_GAMEOBJECT(PB_Lune, ObjectType::Player)

public:
    PB_Lune(const ComPtr<Device>& device, const ComPtr<Context>& context);
    PB_Lune(const PB_Lune& prototype);
    virtual ~PB_Lune() override = default;

protected:
    HairChainPhysicsDesc Get_HairChainPhysicsDesc() const override;

    const wchar_t* Get_BodyModelTag() const override { return L"Model_LuneBody"; }
    const wchar_t* Get_HeadModelTag() const override { return L"Model_LuneHead"; }
    const wchar_t* Get_HairModelTag() const override { return L"Model_LuneHair"; }

    bool Try_GetWeaponAttackDesc(WeaponAttachDesc& outDesc) const override;

public:
    static Shared<PB_Lune> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END

