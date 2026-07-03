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

    HRESULT Ready_BodyModifiers(Body_Player* body, const wchar_t* bodyModelTag) override;
    HRESULT Ready_HairModifiers(Hair_Player* hair, const wchar_t* hairModelTag) override;

    void Get_WeaponAttachDescs(vector<WeaponAttachDesc>& outDescs) const override;

    HeroId Get_HeroId() const override { return HeroId::Lune; }

public:
    static Shared<PB_Lune> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END

