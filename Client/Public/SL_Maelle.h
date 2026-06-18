#pragma once

#include "Character_WorldBase.h"
#include "HairChainPhysics.h"
#include "PlayerStatCom.h"

NS_BEGIN(Engine)
class MovementCom;
NS_END

NS_BEGIN(Client)

class Body_Player;
class Head_Player;
class Hair_Player;
class EffectCom;
struct CostumePartLocalTransform;
struct CostumePreset;

class SL_StateMachineCom;
class SL_TargetCom;
struct SL_MoveRequest;

class SL_Maelle final : public Character_WorldBase
{
    GENERATED_GAMEOBJECT(SL_Maelle, ObjectType::Player)

public:
    SL_Maelle(const ComPtr<Device>& device, const ComPtr<Context>& context);
    SL_Maelle(const SL_Maelle& prototype);
    ~SL_Maelle() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;
    void    Priority_Update(float timeDelta) override;
    void    Update(float timeDelta) override;
    void    Late_Update(float timeDelta) override;
    HRESULT Render() override;
    HRESULT Render_Shadow() override;

public:
    void    Apply_MoveInput(const SL_MoveRequest& request, float timeDelta);
    void    Stop_Move();

    void    Set_CombatLocked(bool locked) { _combatLocked = locked; }
    bool    Is_CombatLocked() const { return _combatLocked; }

    void    Set_InvincibleTime(float value) { _invincibleRemain = max(0.f, value); }
    bool    Is_Invincible() const { return _invincibleRemain > 0.f; }

    void    Apply_Damage(float damage);
    float   Get_Hp() const { return _hp; }
    bool    Is_Dead() const { return _hp <= 0.f; }

    Body_Player* Get_Body() const { return _body.get(); }
    void    Reset_CombatHp(float maxHp);

    HRESULT Apply_WorldStatSnapshot(const PlayerStatCom::BattleSyncSnapshot& snapshot);
	void    Apply_WorldSkillKeys(const vector<string>& skillKeys);
    bool    Try_GetEquippedSkill(uint32 slotIndex, Skill_Data& outSkillData) const;
    HRESULT Rebuild_WeaponPartFromCurrentStat();

public:


private:
    HRESULT Ready_Components();
    HRESULT Ready_PartObjects();
    HRESULT Ready_WeaponPart(Body_Player* body);
    const CostumePreset* Get_CurrentSoulLikeCostume() const;
    const wchar_t* Select_CostumeModelTag(const wstring& costumeModelTag, const wchar_t* defaultModelTag) const;
    void Apply_CostumePartTransform(const wstring& partTag, const CostumePartLocalTransform& fallbackTransform);
    bool Should_ApplySerializedPart(const wstring& partTag, const json& partObjectJson) const override;
    HairChainPhysicsDesc Get_MaelleHairChainPhysicsDesc() const;

private:
    Shared<MovementCom>         _movementCom{};
    Shared<EffectCom>           _effectCom{};
    Shared<PlayerStatCom>       _combatStatCom{};
    Shared<SL_StateMachineCom>  _stateMachineCom;
    Shared<SL_TargetCom>        _targetCom;

    Shared<Body_Player> _body{};
    Shared<Head_Player> _head{};
    Shared<Hair_Player> _hair{};

    float _hp = 100.f;
    float _invincibleRemain = 0.f;
    bool _combatLocked = false; // 공격/피격처럼 이동 입력을 막아야 하는 전투 상태에서만 true로 둔다.

public:
    static Shared<SL_Maelle> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
