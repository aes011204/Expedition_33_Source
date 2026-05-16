#pragma once

#include "Player_BattleBase.h"

NS_BEGIN(Client)

class Battle_Maelle final : public Player_BattleBase
{
    GENERATED_GAMEOBJECT(Battle_Maelle, ObjectType::Player)

public:
    Battle_Maelle(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Battle_Maelle(const Battle_Maelle& prototype);
    virtual ~Battle_Maelle() override = default;

protected:
    HairChainPhysicsDesc Get_HairChainPhysicsDesc() const override;

    const wchar_t* Get_BodyModelTag() const override { return L"Model_MaelleBody"; }
    const wchar_t* Get_HeadModelTag() const override { return L"Model_MaelleHead"; }
    const wchar_t* Get_HairModelTag() const override { return L"Model_MaelleHair"; }

public:
    static Shared<Battle_Maelle> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END

