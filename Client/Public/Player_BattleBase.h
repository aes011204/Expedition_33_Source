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
    struct WeaponAttachDesc
    {
        const wchar_t*  modelTag = nullptr;     // 무기를
        const char*     socketName = nullptr;   // 어디 소켓에 붙일건지
    };

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

    // 무기가 없다면 장착 X
    virtual bool Try_GetWeaopnAttackDesc(WeaponAttachDesc& outDesc) const { return false; }

    // body가 만들어진 뒤, 캐릭터별 추가 파트
    virtual HRESULT Ready_WeaponPart(Body_Player* body);

protected:
    Shared<PlayerStatCom>   _combatStatCom;

public:

};

NS_END
