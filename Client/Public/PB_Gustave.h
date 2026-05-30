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

    // 구스타브 수염
    virtual const wchar_t* Get_BeardModelTag() const { return L"Model_GustaveBeard"; }
    virtual const wchar_t* Get_MustacheModelTag() const { return L"Model_GustaveMustache"; }

    void Get_WeaponAttachDescs(vector<WeaponAttachDesc>& outDescs) const override;

public:
    static Shared<PB_Gustave> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;

protected:


};

NS_END

