#pragma once

#include "Player_BattleBase.h"

NS_BEGIN(Client)

class PB_Gustave final : public Player_BattleBase
{
    GENERATED_GAMEOBJECT(PB_Gustave, ObjectType::Player)

public:
    PB_Gustave(const ComPtr<Device>& device, const ComPtr<Context>& context);
    PB_Gustave(const PB_Gustave& prototype);
    virtual ~PB_Gustave() override = default;

protected:
    HairChainPhysicsDesc Get_HairChainPhysicsDesc() const override;

    const wchar_t* Get_BodyModelTag() const override { return L"Model_GustaveBody"; }
    const wchar_t* Get_HeadModelTag() const override { return L"Model_GustaveHead"; }
    const wchar_t* Get_HairModelTag() const override { return L"Model_GustaveHair"; }

    bool Try_GetWeaopnAttackDesc(WeaponAttachDesc& outDesc) const override;

public:
    static Shared<PB_Gustave> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
protected:
};

NS_END

