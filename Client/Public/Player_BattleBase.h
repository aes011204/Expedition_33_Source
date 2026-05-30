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
class EffectCom;
class BattleMechanismCom;

class Player_BattleBase : public Character_BattleBase
{
public:
    struct WeaponAttachDesc
    {
        wstring         partTag{};              // 어떤 파트 태그로 생성할지. 오른손은 기존 Part_Weapon 유지
        string          weaponPrefabName{};     // 어떤 무기 프리펩을 세팅할건지
        const char* socketName = nullptr;   // 어디 소켓에 붙일건지
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

    json    To_Json() const override;
    void    From_Json(const json& data) override;

protected:
    HRESULT Ready_Components();
    HRESULT Ready_PartObjects();

    virtual HairChainPhysicsDesc Get_HairChainPhysicsDesc() const { return {}; }

    virtual const wchar_t* Get_BodyModelTag() const = 0;
    virtual const wchar_t* Get_HeadModelTag() const = 0;
    virtual const wchar_t* Get_HairModelTag() const = 0;

    // 구스타브 수염
    virtual const wchar_t* Get_BeardModelTag() const { return nullptr; }
    virtual const wchar_t* Get_MustacheModelTag() const { return nullptr; }

    // 캐릭터별 무기 슬롯 목록 세팅.
    virtual void Get_WeaponAttachDescs(vector<WeaponAttachDesc>& outDesc) const {}

    // body가 만들어진 뒤, 캐릭터별 추가 파트
    virtual HRESULT Ready_WeaponParts(Body_Player* body);

protected:
    Shared<PlayerStatCom>       _combatStatCom;
    Shared<EffectCom>           _effectCom;
    Shared<BattleMechanismCom>  _battleMechanismCom;

};

NS_END
