#pragma once

#include "SL_Base.h"

NS_BEGIN(Engine)
class AIController;
class ColliderCom;
class ModelCom;
class MovementCom;
NS_END

NS_BEGIN(Client)
class Mon_Dualliste_Body;
class Mon_Dualliste_Sword_L;
class Mon_Dualliste_Sword_R;
class MonsterStatCom;
class SL_Mon_StateMachineCom;
class SL_DuallisteCloneSkillManager;
class EffectCom;

class SL_Dualliste final : public SL_Base
{
    GENERATED_GAMEOBJECT(SL_Dualliste, ObjectType::Monster)

public:
    enum class Anim
    {
        Intro,
        Idle,
        Walk,
        Run,
        DashIntro,
        Dash_1,
        DashBack_Spain,
        DashIn_Stab,
        PhaseChange,
        Stun_Start,
        Stun_Idle,
        Stun_End,
        Attack_Combo_1,
        Attack_Combo_2,
        Hit,
        Dead,
        SingleSlash_1, SingleSlash_2, SingleSlash_3,
        Att_Projectile,
        DashIn_Stab_Estoc
    };

public:
    SL_Dualliste(const ComPtr<Device>& device, const ComPtr<Context>& context);
    SL_Dualliste(const SL_Dualliste& prototype);
    ~SL_Dualliste() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void BeginPlay() override;
    void Priority_Update(float timeDelta) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;
    HRESULT Render_Shadow() override;

public:
    void OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    Shared<CombatStatCom> Get_Stat() const override;
    Shared<ColliderCom> Get_AttackCollider() const;
    Shared<ColliderCom> Get_AttackColliderL() const;
    Shared<ColliderCom> Get_AttackColliderR() const;
    void Set_AttackHitAnimOverride(const string& colliderTag, SL_HitAnimOverride hitAnimOverride) override;
    void Clear_AttackHitAnimOverride(const string& colliderTag) override;
    Shared<MovementCom> Get_MovementCom() const;
    Shared<ModelCom> Get_ModelCom() const;
    bool Try_GetSocketWorldPosition(const string& socketName, Vec3& outWorldPosition) const;
    bool Play_Anim(Anim anim, float blendDuration, bool loop);
    bool Play_Anim(Anim anim, float blendDuration, bool loop, float startFrame);
    void Set_AnimPaused(bool paused);
    void Set_AnimTimeSec(float timeSec);
    void Refresh_AnimPose();
    bool Is_AnimFinished() const;
    bool Is_Dead() const override;
    bool Is_DamageBlocked() const override { return Is_PhaseChangeInvincible(); }
    void Set_IntroFin(bool isDone) { _isIntroDone = isDone; };
    void Apply_Damage(const SL_DamageInfo& damageInfo) override;
    bool Change_State(SL_MonState state);
    int Get_CurrentPhase() const { return _currentPhase; }
    bool CanUseLeftSword() const { return _currentPhase >= 2; }
    bool Is_PhaseChangeFinished() const { return _phaseChangeFinished; }
    bool Is_PhaseChangeInvincible() const { return _isPhaseChangeInvincible; }
    bool Should_SkipPhaseChangeStunStart() const { return _skipPhaseChangeStunStart; }
    void Set_CinematicDriven(bool cinematicDriven);
    bool Is_CinematicDriven() const { return _isCinematicDriven; }
    Shared<SL_DuallisteCloneSkillManager> Get_CloneSkillManager() const { return _cloneSkillManager; }
    bool Notify_CloneSkillAttack(GameObject* notifyOwner) const;
    void Cache_PlayerAnchor(const Vec3& playerPosition, const Vec3& playerRight, const Vec3& playerUp, const Vec3& playerForward);
    bool Try_GetCachedPlayerAnchor(Vec3& outPlayerPosition, Vec3& outPlayerRight, Vec3& outPlayerUp, Vec3& outPlayerForward) const;
    void Clear_CachedPlayerAnchor();
    void Set_CloneVisualOnly(bool isCloneVisualOnly);
    bool Can_EnterPhaseChange() const;
    bool Try_EnterPhaseChangeFromStun();
    void Begin_PhaseChangeTransition();
    void Finish_PhaseChange();
    void End_PhaseChangeTransition();
    void Set_ActionLocked(bool isActionLock);
protected:
    HRESULT Ready_Components();
    HRESULT Ready_PartObjects();
    bool Should_ApplySerializedPart(const wstring& partTag, const json& partObjectJson) const override;
    bool Try_EnterPhaseChange();
    bool Should_TriggerPhaseChangeNow() const;
    void Apply_PhaseVisualState();
    void Update_BlackBoard();
    void Update_StunSystem(float timeDelta);
    float Calculate_BreakDamage(AttackType attackType) const;
    void Clear_BreakState();

private:
    Shared<MonsterStatCom> _combatStat = {};
    Shared<ColliderCom> _attackCom = {};
    Shared<ColliderCom> _attackRCom = {};
    Shared<ColliderCom> _attackLCom = {};
    Shared<EffectCom> _effectCom = {};

    SL_HitAnimOverride _attackHitAnimOverride = SL_HitAnimOverride::Hit_Light;
    SL_HitAnimOverride _attackRHitAnimOverride = SL_HitAnimOverride::Hit_Light;
    SL_HitAnimOverride _attackLHitAnimOverride = SL_HitAnimOverride::Hit_Light;
    Shared<AIController>         _aiCtrl;
    Shared<MovementCom>         _movementCom = {};
    Shared<SL_Mon_StateMachineCom> _stateMachineCom = {};
    Shared<Mon_Dualliste_Body> _body = {};
    Shared<Mon_Dualliste_Sword_L> _swordL = {};
    Shared<Mon_Dualliste_Sword_R> _swordR = {};
    Anim _currentAnim = Anim::Idle;
    bool _isIntroDone = false;
    int _currentPhase = 1;
    bool _phaseChangeTriggered = false;
    bool _phaseChangeFinished = false;
    bool _isPhaseChangeInvincible = false;
    bool _skipPhaseChangeStunStart = false;
    bool _isCinematicDriven = false;
    bool _restorePhysXCCTAfterCinematic = false;
    float _stunElapsed = 0.f;
    float _breakRecoverPerSec = 4.5f;

    float kCanAttackDistance = 3.0f;
    float kPhase2HpThreshold = 0.5f;
    Shared<SL_DuallisteCloneSkillManager> _cloneSkillManager = {};
    bool _isCloneVisualOnly = false;
    Vec3 _cachedPlayerPos = Vec3::Zero;
    Vec3 _cachedPlayerRight = Vec3::Right;
    Vec3 _cachedPlayerUp = Vec3::Up;
    Vec3 _cachedPlayerForward = Vec3::Look;
    bool _hasCachedPlayerAnchor = false;
   


public:
    static Shared<SL_Dualliste> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
