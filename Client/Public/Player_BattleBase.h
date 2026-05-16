#pragma once

#include "Character_BattleBase.h"

NS_BEGIN(Engine)
class ColliderCom;
NS_END

NS_BEGIN(Client)

class PlayerStatCom;
class Body_Player;
class Head_Player;
class Hair_Player;

class Player_BattleBase : public Character_BattleBase
{
public:
    Player_BattleBase(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Player_BattleBase(const Player_BattleBase& prototype);
    virtual ~Player_BattleBase() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;
    void    Update(float timeDelta) override;
    void    Late_Update(float timeDelta) override;

protected:
    HRESULT Ready_Components();
    HRESULT Ready_PartObjects();

    virtual HairChainPhysicsDesc Get_HairChainPhysicsDesc() const { return {}; }

    virtual const wchar_t* Get_BodyModelTag() const = 0;
    virtual const wchar_t* Get_HeadModelTag() const = 0;
    virtual const wchar_t* Get_HairModelTag() const = 0;

protected:
    Shared<PlayerStatCom>   _combatStatCom;

};

NS_END
