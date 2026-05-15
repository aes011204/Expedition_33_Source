#pragma once

#include "Player_BattleBase.h"

NS_BEGIN(Client)

class Battle_Gustave final : public Player_BattleBase
{
    GENERATED_GAMEOBJECT(Battle_Gustave, ObjectType::Player)

public:
    Battle_Gustave(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Battle_Gustave(const Battle_Gustave& prototype);
    virtual ~Battle_Gustave() override = default;

protected:
    HairChainPhysicsDesc Get_HairChainPhysicsDesc() const override;

    const wchar_t* Get_BodyModelTag() const override { return L"Model_GustaveBody"; }
    const wchar_t* Get_HeadModelTag() const override { return L"Model_GustaveHead"; }
    const wchar_t* Get_HairModelTag() const override { return L"Model_GustaveHair"; }

public:
    static Shared<Battle_Gustave> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END

