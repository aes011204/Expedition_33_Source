#pragma once

#include "Player_BattleBase.h"

NS_BEGIN(Client)

class Battle_Lune final : public Player_BattleBase
{
    GENERATED_GAMEOBJECT(Battle_Lune, ObjectType::Player)

public:
    Battle_Lune(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Battle_Lune(const Battle_Lune& prototype);
    virtual ~Battle_Lune() override = default;

protected:
    HairChainPhysicsDesc Get_HairChainPhysicsDesc() const override;

    const wchar_t* Get_BodyModelTag() const override { return L"Model_LuneBody"; }
    const wchar_t* Get_HeadModelTag() const override { return L"Model_LuneHead"; }
    const wchar_t* Get_HairModelTag() const override { return L"Model_LuneHair"; }

public:
    static Shared<Battle_Lune> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END

