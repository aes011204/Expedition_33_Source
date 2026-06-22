#pragma once

#include "SL_Base.h"
#include "HairChainPhysics.h"
#include "PlayerStatCom.h"

NS_BEGIN(Engine)
class MovementCom;
class ColliderCom;
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

class SL_Maelle final : public SL_Base
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

    void    OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;

    Shared<CombatStatCom> Get_Stat() const override;
    Shared<ColliderCom> Get_AttackCollider() const;
    bool    Is_Dead() const override;
    void    Apply_Damage(const SL_DamageInfo& damageInfo) override;

public:
    void    Apply_MoveInput(const SL_MoveRequest& request, float timeDelta);
    void    Stop_Move();

    void    Set_CombatLocked(bool locked) { _combatLocked = locked; }
    bool    Is_CombatLocked() const { return _combatLocked; }

    void    Set_InvincibleTime(float value) { _invincibleRemain = max(0.f, value); }
    bool    Is_Invincible() const { return _invincibleRemain > 0.f; }

    void    Open_ParryWindow();
    void    Close_ParryWindow();
    void    Reset_ParryConsumed();
    bool    Try_ConsumeParry(const Vec3& impactWorldPos);
    bool    Is_ParryWindowOpen() const { return _parryWindowOpen; }

    Body_Player* Get_Body() const { return _body.get(); }
    void    Reset_CombatHp(float maxHp);

    HRESULT Apply_WorldStatSnapshot(const PlayerStatCom::BattleSyncSnapshot& snapshot);
	void    Apply_WorldSkillKeys(const vector<string>& skillKeys);
    bool    Try_GetEquippedSkill(uint32 slotIndex, Skill_Data& outSkillData) const;
    HRESULT Rebuild_WeaponPartFromCurrentStat();

private:
    HRESULT Ready_Components();
    HRESULT Ready_PartObjects();
    HRESULT Ready_WeaponPart(Body_Player* body);

private:
    const CostumePreset* Get_CurrentSoulLikeCostume() const;
    const wchar_t*       Select_CostumeModelTag(const wstring& costumeModelTag, const wchar_t* defaultModelTag) const;
    void                 Apply_CostumePartTransform(const wstring& partTag, const CostumePartLocalTransform& fallbackTransform);
    bool                 Should_ApplySerializedPart(const wstring& partTag, const json& partObjectJson) const override;
    HairChainPhysicsDesc Get_MaelleHairChainPhysicsDesc() const;

	// 패링이 앞에서 데미지 들어왔는지
	bool				 Is_ParryFrontHit(const Vec3& impactWorldPos) const;
    void                 Begin_HitPush(const SL_DamageInfo& damageInfo);
    void                 Update_HitPush(float timeDelta);

private:
    Shared<MovementCom>         _movementCom{};
    Shared<EffectCom>           _effectCom{};
    Shared<PlayerStatCom>       _combatStatCom{};
    Shared<SL_StateMachineCom>  _stateMachineCom;
    Shared<SL_TargetCom>        _targetCom;

    Shared<Body_Player>         _body{};
    Shared<Head_Player>         _head{};
    Shared<Hair_Player>         _hair{};

    Shared<ColliderCom>         _attackColliderCom{};

    float _invincibleRemain = 0.f;
    bool  _combatLocked = false; // 공격/피격처럼 이동 입력을 막아야 하는 전투 상태에서만 true
    bool  _parryWindowOpen = false;
    bool  _parryConsumed = false;

private: /* 피격 밀림 */
    Vec3  _hitPushDir = Vec3::Zero;
    float _hitPushDistance = 0.f;
    float _hitPushDuration = 0.f;
    float _hitPushElapsed = 0.f;

public:
    static Shared<SL_Maelle> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
