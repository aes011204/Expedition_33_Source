#pragma once

#include "Player_BattleBase.h"

NS_BEGIN(Client)

class PB_Maelle final : public Player_BattleBase
{
    GENERATED_GAMEOBJECT(PB_Maelle, ObjectType::Player)

public:
    PB_Maelle(const ComPtr<Device>& device, const ComPtr<Context>& context);
    PB_Maelle(const PB_Maelle& prototype);
    virtual ~PB_Maelle() override = default;

protected:
    HairChainPhysicsDesc Get_HairChainPhysicsDesc() const override;

    const wchar_t* Get_BodyModelTag() const override { return L"Model_MaelleBody"; }
    const wchar_t* Get_HeadModelTag() const override { return L"Model_MaelleHead"; }
    const wchar_t* Get_HairModelTag() const override { return L"Model_MaelleHair"; }

    bool Try_GetWeaponAttackDesc(WeaponAttachDesc& outDesc) const override;

public:
    static Shared<PB_Maelle> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END

