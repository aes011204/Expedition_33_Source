#include "SL_Dualliste.h"
#include "Bounding_OBB.h"
#include "Client_Constants.h"
#include "WeaponPrefabLoader.h"
#include "ColliderCom.h"
#include "ModelCom.h"
#include "Mon_Dualliste_Body.h"
#include "Mon_Dualliste_Sword_L.h"
#include "Mon_Dualliste_Sword_R.h"
#include "MonsterStatCom.h"
#include "SL_DamageSystem.h"
#include "SL_Mon_StateMachineCom.h"
#include "AIController.h"
#include "ClientScreenFxManager.h"
#include "BlackBoard.h"
#include "ClientInstance.h"
#include "SoulLikeController.h"
#include "SL_Maelle.h"
#include "MovementCom.h"
#include "EffectCom.h"
#include "SL_DuallisteCloneSkillManager.h"

REGISTER_GLOBAL_ENUM(SL_Dualliste::Anim, "SL_DuallisteAnim")

IMPLEMENT_REFLECTION(SL_Dualliste)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "SL_Dualliste";
    info.category = "SoulLike";

    return true;
}

SL_Dualliste::SL_Dualliste(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : SL_Base(device, context)
{
}

SL_Dualliste::SL_Dualliste(const SL_Dualliste& prototype)
    : SL_Base(prototype)
{
}

HRESULT SL_Dualliste::Initialize_Prototype()
{
    return SL_Base::Initialize_Prototype();
}

HRESULT SL_Dualliste::Initialize(void* arg)
{
    CHECK_FAILED(SL_Base::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    return S_OK;
}

bool SL_Dualliste::Should_ApplySerializedPart(const wstring& partTag, const json& partObjectJson) const
{
    UNREFERENCED_PARAMETER(partObjectJson);

    return partTag != TEXT("Part_Sword_L") &&
        partTag != TEXT("Part_Sword_R");
}

void SL_Dualliste::BeginPlay()
{
    SL_Base::BeginPlay();

    _currentPhase = 1;
    _phaseChangeTriggered = false;
    _phaseChangeFinished = false;
    Set_ActionLocked(false);

    if (_aiCtrl)
    {
        if (const Shared<BlackBoard> bb = _aiCtrl->Get_BlackBoard())
            bb->Set_Value<int>("CurrentPhase", _currentPhase);
    }

    if (_attackRCom)
    {
        _attackRCom->Set_IsActive(false);
        _attackRCom->Clear_Overlap();
    }

    if (_attackLCom)
    {
        _attackLCom->Set_IsActive(false);
        _attackLCom->Clear_Overlap();
    }

    Apply_PhaseVisualState();

    if (!_isCloneVisualOnly && _cloneSkillManager == nullptr)
        _cloneSkillManager = SL_DuallisteCloneSkillManager::Create(GetSharedPtr<SL_Dualliste>());

    if (!_isCloneVisualOnly && _stateMachineCom)
        _stateMachineCom->Change_State(SL_MonState::Intro);

    if (_movementCom)
    {
        _movementCom->Sync_RuntimePositionToTransform();
        _movementCom->Stop_AllMovement();
        _movementCom->Clear_FollowMoveCommand();
        _movementCom->Apply_Input({});
    }
}

void SL_Dualliste::Priority_Update(float timeDelta)
{
    SL_Base::Priority_Update(timeDelta);
}

void SL_Dualliste::Update(float timeDelta)
{
    if (_combatStat == nullptr || _body == nullptr)
        return;

    if (CLIENT->Is_BattleActive())
        return;

    if (_cloneSkillManager)
        _cloneSkillManager->Update(timeDelta);

    if (_isCloneVisualOnly)
    {
        _body->Play_Animation(timeDelta);

        if (_effectCom)
            _effectCom->Update(timeDelta);

        SL_Base::Update(timeDelta);
        return;
    }

    Update_StunSystem(timeDelta);

    Try_EnterPhaseChange();

    Update_BlackBoard();

    const bool isPhaseChanging =
        _stateMachineCom &&
        _stateMachineCom->Get_CurrentMonStateType() == SL_MonState::PhaseChange;

    if (_aiCtrl && _isIntroDone && !isPhaseChanging)
        _aiCtrl->Update(timeDelta);

    if (_stateMachineCom && CLIENT->Get_ScreenFxManager()->Is_BattleInCounterFxFinished())
        _stateMachineCom->Update_CurrentState(timeDelta);

    if (_movementCom)
    {
        const float moveSpeed = max(_transformCom->Get_SpeedPerSec(), 0.f);
        _movementCom->Set_WalkSpeed(moveSpeed);
        _movementCom->Set_RunSpeed(moveSpeed);
        _movementCom->Set_SprintSpeed(moveSpeed);

        if (_isIntroDone && _aiCtrl && _aiCtrl->Has_MoveIntent() && !Is_Dead())
        {
            MovementCom::FollowMoveCommand command{};
            command.enabled = true;
            command.worldDir = _aiCtrl->Get_MoveWorldDir();
            command.rotateFollow = false;

            _movementCom->Set_FollowMoveCommand(command);
            _movementCom->Apply_Input({});
            _movementCom->Update_Movement(timeDelta);
        }
        else
        {
            _movementCom->Clear_FollowMoveCommand();
            _movementCom->Apply_Input({});
            _movementCom->Stop_HorizontalMovement();
            _movementCom->Update_Movement(timeDelta);
        }
    }

    if (_body)
        _body->Play_Animation(timeDelta);

    if (_effectCom)
        _effectCom->Update(timeDelta);

    SL_Base::Update(timeDelta);
}

void SL_Dualliste::Late_Update(float timeDelta)
{
    SL_Base::Late_Update(timeDelta);

    Update_Culling(Vec3(1.f, 2.f, 1.f));
}

HRESULT SL_Dualliste::Render()
{
    return SL_Base::Render();
}

HRESULT SL_Dualliste::Render_Shadow()
{
    return SL_Base::Render_Shadow();
}

void SL_Dualliste::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    SL_Base::OnBeginOverlap(self, other);

    if (self == nullptr || other == nullptr)
        return;

    if (self.get() != _attackCom.get() &&
        self.get() != _attackRCom.get() &&
        self.get() != _attackLCom.get())
        return;

    if (other->Get_CollisionPreset() != Collision_Preset::SL_PlayerBody)
        return;

    const Shared<SL_Base> target = dynamic_pointer_cast<SL_Base>(other->Get_Owner());
    if (target == nullptr || target.get() == this || target->Is_Dead())
        return;

    if (Shared<SL_Maelle> maelle = dynamic_pointer_cast<SL_Maelle>(target))
    {
        const Shared<TransformCom> maelleTransform = maelle->Get_Transform();
        const Vec3 maelleWorldPos = maelleTransform
            ? maelleTransform->Get_WorldPosition()
            : Vec3::Zero;

        const Vec3 attackerWorldPos = _transformCom
            ? _transformCom->Get_WorldPosition()
            : maelleWorldPos;

        Vec3 effectDir = attackerWorldPos - maelleWorldPos;
        effectDir.y = 0.f;

        if (effectDir.LengthSquared() > 0.0001f)
            effectDir.Normalize();
        else if (maelleTransform)
            effectDir = maelleTransform->Get_WorldForward();

        effectDir.y = 0.f;
        if (effectDir.LengthSquared() > 0.0001f)
            effectDir.Normalize();

        Vec3 parryEffectWorldPos = maelleWorldPos + effectDir * 0.8f;
        parryEffectWorldPos.y += 1.0f;

        auto attacker = GetSharedPtr<GameObject>();

        if (maelle->Try_ConsumeParry(attacker, attackerWorldPos, parryEffectWorldPos))
        {
            self->Set_IsActive(false);
            self->Clear_Overlap();
            return;
        }

        if (const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController())
        {
            const Shared<SL_Base> attacker = dynamic_pointer_cast<SL_Base>(GetSharedPtr<GameObject>());
            if (controller->Try_ConsumeJumpCounterDodge(attacker, maelle))
            {
                self->Set_IsActive(false);
                self->Clear_Overlap();
                return;
            }
        }
    }

    SL_DamageInfo damageInfo{};
    if (!SL_DamageSystem::Apply_Damage(
        GetSharedPtr<GameObject>(),
        _combatStat,
        target,
        target->Get_Stat(),
        1.f,
        AttackType::BaseAttack,
        damageInfo))
    {
        return;
    }

    if (self.get() == _attackCom.get())
        damageInfo.hitAnimOverride = _attackHitAnimOverride;
    else if (self.get() == _attackLCom.get())
        damageInfo.hitAnimOverride = _attackLHitAnimOverride;
    else
        damageInfo.hitAnimOverride = _attackRHitAnimOverride;
    target->Apply_Damage(damageInfo);

    GAME->Play_SoundCue(Constants::SoundCue::Duallist_Attack);
}

Shared<CombatStatCom> SL_Dualliste::Get_Stat() const
{
    return _combatStat;
}

Shared<ColliderCom> SL_Dualliste::Get_AttackCollider() const
{
    return _attackCom;
}

Shared<ColliderCom> SL_Dualliste::Get_AttackColliderL() const
{
    return _attackLCom;
}
Shared<ColliderCom> SL_Dualliste::Get_AttackColliderR() const
{
    return _attackRCom;
}

void SL_Dualliste::Set_AttackHitAnimOverride(const string& colliderTag, SL_HitAnimOverride hitAnimOverride)
{
    if (colliderTag == "Com_SL_MonsterAttack")
    {
        _attackHitAnimOverride = hitAnimOverride;
        return;
    }

    if (colliderTag == "Com_SL_MonsterSword_L")
    {
        _attackLHitAnimOverride = hitAnimOverride;
        return;
    }

    if (colliderTag == "Com_SL_MonsterSword_R")
        _attackRHitAnimOverride = hitAnimOverride;
}

void SL_Dualliste::Clear_AttackHitAnimOverride(const string& colliderTag)
{
    if (colliderTag == "Com_SL_MonsterAttack")
    {
        _attackHitAnimOverride = SL_HitAnimOverride::Hit_Light;
        return;
    }

    if (colliderTag == "Com_SL_MonsterSword_L")
    {
        _attackLHitAnimOverride = SL_HitAnimOverride::Hit_Light;
        return;
    }

    if (colliderTag == "Com_SL_MonsterSword_R")
        _attackRHitAnimOverride = SL_HitAnimOverride::Hit_Light;
}

Shared<MovementCom> SL_Dualliste::Get_MovementCom() const
{
    return _movementCom;
}

Shared<ModelCom> SL_Dualliste::Get_ModelCom() const
{
    return _body ? _body->Get_Model() : nullptr;
}

bool SL_Dualliste::Play_Anim(Anim anim, float blendDuration, bool loop)
{
    if (_body == nullptr)
        return false;

    if (_currentAnim == anim && loop)
        return true;

    if (_body->Set_AnimationByState(ETOI(anim), blendDuration, loop))
    {
        _currentAnim = anim;
        return true;
    }

    return false;
}

bool SL_Dualliste::Play_Anim(Anim anim, float blendDuration, bool loop, float startFrame)
{
    if (_body == nullptr)
        return false;

    if (_body->Set_AnimationByState(ETOI(anim), blendDuration, loop, startFrame))
    {
        _currentAnim = anim;
        return true;
    }

    return false;
}

void SL_Dualliste::Set_AnimPaused(bool paused)
{
    if (_body == nullptr || _body->Get_Model() == nullptr)
        return;

    _body->Get_Model()->Set_AnimationPaused(paused);
}

void SL_Dualliste::Set_AnimTimeSec(float timeSec)
{
    if (_body == nullptr || _body->Get_Model() == nullptr)
        return;

    _body->Get_Model()->Set_CurrentAnimationTimeSec(timeSec);
}

void SL_Dualliste::Refresh_AnimPose()
{
    if (_body == nullptr || _body->Get_Model() == nullptr)
        return;

    _body->Get_Model()->Refresh_CurrentAnimationPose();
}

bool SL_Dualliste::Is_AnimFinished() const
{
    return _body != nullptr && _body->Is_AnimationFinished();
}

bool SL_Dualliste::Is_Dead() const
{
    return _combatStat == nullptr || _combatStat->Get_AliveStatus() == EAliveStatus::Dead;
}

void SL_Dualliste::Apply_Damage(const SL_DamageInfo& damageInfo)
{
    if (damageInfo.target.get() != this)
        return;

    if (_combatStat == nullptr || _stateMachineCom == nullptr)
        return;

    if (_isPhaseChangeInvincible)
        return;

    if (Is_Dead())
    {
        Set_ActionLocked(false);
        _stateMachineCom->Change_State(SL_MonState::Dead);
        return;
    }

    if (_combatStat->IsBreak())
        return;

    _combatStat->Add_Break(Calculate_BreakDamage(damageInfo.attackType));

    if (_combatStat->Get_CurBreak() >= _combatStat->Get_MaxBreak())
    {
        if (Should_TriggerPhaseChangeNow())
        {
            Clear_BreakState();
            Try_EnterPhaseChange();
            return;
        }

        _combatStat->SetBreak(true);

        // 스턴 이펙트
        {
            PlayEffectDesc effectDesc{};
            effectDesc.effectName = Constants::Effect::BreakEffect;
            effectDesc.worldPosition = _transformCom->Get_WorldPosition() + Vec3(0.f, 1.7f, 1.5f);
            effectDesc.scale = Vec3(1.f, 1.f, 1.f);
            CLIENT->Play_Effect(effectDesc);
        }

        Set_ActionLocked(false);
        _stateMachineCom->Change_State(SL_MonState::Stun);

        return;
    }

    GAME->Play_SoundCue(Constants::SoundCue::Duallist_Pain);

    _stateMachineCom->Change_State(SL_MonState::Hit);
}

void SL_Dualliste::Update_BlackBoard()
{
    if (_aiCtrl == nullptr || _transformCom == nullptr)
        return;

    const Shared<BlackBoard> bb = _aiCtrl->Get_BlackBoard();
    if (bb == nullptr)
        return;

    if (!bb->Has_Value("CurrentPhase"))
        bb->Set_Value<int>("CurrentPhase", _currentPhase);

    bb->Set_Value<bool>("IsDead", Is_Dead());
    bb->Set_Value<bool>("IsStun", _combatStat && _combatStat->IsBreak() && !_isPhaseChangeInvincible);

    const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController();
    const Shared<SL_Maelle> player = controller ? controller->Get_Player() : nullptr;
    if (player == nullptr || player->Get_Transform() == nullptr)
    {
        bb->Set_Value<bool>("CanAttack", false);
        return;
    }

    const Vec3 targetPos = player->Get_Transform()->Get_WorldPosition();
    bb->Set_Value<Vec3>("TargetPos", targetPos);

    const Vec3 myPos = _transformCom->Get_WorldPosition();
    const Vec3 toTarget = targetPos - myPos;
    const float distanceToTarget = toTarget.Length();
    bb->Set_Value<float>("TargetDistance", distanceToTarget);
    bb->Set_Value<bool>("CanAttack", distanceToTarget <= kCanAttackDistance);

    Vec3 forward = _transformCom->Get_WorldForward();
    forward.y = 0.f;

    Vec3 flatToTarget = toTarget;
    flatToTarget.y = 0.f;

    float forwardDot = 1.f;
    float angleDeg = 0.f;

    if (forward.LengthSquared() > 0.000001f && flatToTarget.LengthSquared() > 0.000001f)
    {
        forward.Normalize();
        flatToTarget.Normalize();

        forwardDot = forward.Dot(flatToTarget);
        forwardDot = clamp(forwardDot, -1.f, 1.f);
        angleDeg = XMConvertToDegrees(acosf(forwardDot));
    }

    bb->Set_Value<float>("TargetForwardDot", forwardDot);
    bb->Set_Value<float>("TargetAngleDeg", angleDeg);

    const float maxHp = max(_combatStat ? _combatStat->Get_StatInfo().maxHp : 0.f, 0.0001f);
    const float hpRatio = _combatStat ? clamp(_combatStat->Get_CurHp() / maxHp, 0.f, 1.f) : 0.f;
    bb->Set_Value<float>("HpRatio", hpRatio);
}

bool SL_Dualliste::Change_State(SL_MonState state)
{
    if (_stateMachineCom == nullptr)
        return false;

    return _stateMachineCom->Change_State(state);
}

bool SL_Dualliste::Notify_CloneSkillAttack(GameObject* notifyOwner) const
{
    if (_cloneSkillManager == nullptr)
        return false;

    return _cloneSkillManager->Notify_Attack(notifyOwner);
}

void SL_Dualliste::Set_CloneVisualOnly(bool isCloneVisualOnly)
{
    _isCloneVisualOnly = isCloneVisualOnly;

    if (!_isCloneVisualOnly)
        return;

    _isIntroDone = false;
    _currentPhase = 2;
    _phaseChangeTriggered = false;
    _phaseChangeFinished = true;
    _isPhaseChangeInvincible = false;
    _skipPhaseChangeStunStart = false;
    Set_ActionLocked(false);
    Apply_PhaseVisualState();

    if (_colliderCom)
    {
        _colliderCom->Set_IsActive(false);
        _colliderCom->Clear_Overlap();
    }

    if (_attackCom)
    {
        _attackCom->Set_IsActive(false);
        _attackCom->Clear_Overlap();
    }

    if (_attackRCom)
    {
        _attackRCom->Set_IsActive(false);
        _attackRCom->Clear_Overlap();
    }

    if (_attackLCom)
    {
        _attackLCom->Set_IsActive(false);
        _attackLCom->Clear_Overlap();
    }

    if (_movementCom)
    {
        _movementCom->Set_Gravity(0.f);
        _movementCom->Set_MoveBackend(MovementCom::MoveBackend::Sweep);
        _movementCom->Stop_AllMovement();
        _movementCom->Clear_FollowMoveCommand();
        _movementCom->Apply_Input({});
    }
}

void SL_Dualliste::Begin_PhaseChangeTransition()
{
    _isPhaseChangeInvincible = true;
    Clear_BreakState();
}

void SL_Dualliste::Finish_PhaseChange()
{
    if (_phaseChangeFinished)
        return;

    _currentPhase = 2;
    _phaseChangeFinished = true;
    Apply_PhaseVisualState();

    if (const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController())
        controller->Play_DuallisteBGM(SoulLikeController::DuallisteBGMPhase::Phase2);

    if (_aiCtrl)
    {
        if (const Shared<BlackBoard> bb = _aiCtrl->Get_BlackBoard())
            bb->Set_Value<int>("CurrentPhase", _currentPhase);
    }
}

void SL_Dualliste::End_PhaseChangeTransition()
{
    _isPhaseChangeInvincible = false;
    _skipPhaseChangeStunStart = false;
}

void SL_Dualliste::Set_ActionLocked(bool isActionLock)
{
    if (_aiCtrl == nullptr)
        return;

    const Shared<BlackBoard> bb = _aiCtrl->Get_BlackBoard();
    if (bb == nullptr)
        return;

    bb->Set_Value<bool>("IsActionLocked", isActionLock);
}

bool SL_Dualliste::Try_EnterPhaseChange()
{
    if (_stateMachineCom == nullptr)
        return false;

    if (!Should_TriggerPhaseChangeNow())
        return false;

    const SL_MonState currentState = _stateMachineCom->Get_CurrentMonStateType();
    if (currentState == SL_MonState::Dead || currentState == SL_MonState::PhaseChange)
        return false;

    if (currentState == SL_MonState::Stun)
        return false;

    _phaseChangeTriggered = true;
    _skipPhaseChangeStunStart = currentState == SL_MonState::Stun;
    Begin_PhaseChangeTransition();
    Set_ActionLocked(false);
    if (_stateMachineCom->Change_State(SL_MonState::PhaseChange))
        return true;

    End_PhaseChangeTransition();
    _skipPhaseChangeStunStart = false;
    _phaseChangeTriggered = false;
    return false;
}

bool SL_Dualliste::Can_EnterPhaseChange() const
{
    return Should_TriggerPhaseChangeNow();
}

bool SL_Dualliste::Try_EnterPhaseChangeFromStun()
{
    if (_stateMachineCom == nullptr)
        return false;

    if (!Should_TriggerPhaseChangeNow())
        return false;

    if (_stateMachineCom->Get_CurrentMonStateType() != SL_MonState::Stun)
        return false;

    Clear_BreakState();

    _phaseChangeTriggered = true;
    _skipPhaseChangeStunStart = true;
    Begin_PhaseChangeTransition();
    Set_ActionLocked(false);
    if (_stateMachineCom->Change_State(SL_MonState::PhaseChange))
        return true;

    End_PhaseChangeTransition();
    _skipPhaseChangeStunStart = false;
    _phaseChangeTriggered = false;
    return false;
}

bool SL_Dualliste::Should_TriggerPhaseChangeNow() const
{
    if (_combatStat == nullptr)
        return false;

    if (_phaseChangeTriggered || _phaseChangeFinished || Is_Dead())
        return false;

    const float maxHp = max(_combatStat->Get_StatInfo().maxHp, 0.0001f);
    const float hpRatio = clamp(_combatStat->Get_CurHp() / maxHp, 0.f, 1.f);
    return hpRatio <= kPhase2HpThreshold;
}

void SL_Dualliste::Apply_PhaseVisualState()
{
    if (_swordR)
        _swordR->Set_Visible(true);

    if (_swordL)
        _swordL->Set_Visible(CanUseLeftSword());

    if (!CanUseLeftSword() && _attackLCom)
    {
        _attackLCom->Set_IsActive(false);
        _attackLCom->Clear_Overlap();
    }
}

void SL_Dualliste::Update_StunSystem(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (_combatStat == nullptr)
        return;

    if (_combatStat->IsBreak())
    {
        return;
    }

    if (_combatStat->Get_CurBreak() <= 0.f)
        return;

    _combatStat->Set_Break(_combatStat->Get_CurBreak() - _breakRecoverPerSec * timeDelta);
}

float SL_Dualliste::Calculate_BreakDamage(AttackType attackType) const
{
    switch (attackType)
    {
    case AttackType::Counter:
        return 45.f;

    case AttackType::ComboLast:
        return 25.f;

    case AttackType::SingleSkill:
    case AttackType::SingleAOE:
        return 20.f;

    case AttackType::Combo:
    case AttackType::FreeAimLight:
        return 15.f;

    case AttackType::BaseAttack:
    case AttackType::FreeAimWeak:
        return 20.f;

    case AttackType::BuffEffect:
    case AttackType::Invalid:
    default:
        return 0.f;
    }
}

void SL_Dualliste::Clear_BreakState()
{
    if (_combatStat == nullptr)
        return;

    _combatStat->SetBreak(false);
    _combatStat->Clear_Break();
}

HRESULT SL_Dualliste::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_AIController"), _aiCtrl), E_FAIL);
    _aiCtrl->Set_Owner(static_pointer_cast<SL_Dualliste>(shared_from_this()));

    MovementCom::MoveDesc moveDesc{};
    moveDesc.walkSpeed = 3.f;
    moveDesc.runSpeed = 6.f;
    moveDesc.sprintSpeed = 6.f;
    moveDesc.turnSpeedDegree = 540.f;
    moveDesc.turnResponse = 10.f;
    moveDesc.jumpVelocity = 0.f;
    moveDesc.gravity = -40.f;
    moveDesc.backend = MovementCom::MoveBackend::PhysXCCT;
    moveDesc.sweep.skinWidth = 0.03f;
    moveDesc.sweep.groundSnapUp = 0.35f;
    moveDesc.sweep.groundSnapDown = 0.65f;
    moveDesc.sweep.walkableNormalY = 0.65f;
    moveDesc.sweep.groundY = 0.f;
    moveDesc.cct.radius = 0.32f;
    moveDesc.cct.height = 1.50f;
    moveDesc.cct.stepOffset = 0.45f;
    moveDesc.cct.contactOffset = 0.05f;
    moveDesc.cct.slopeLimitDeg = 55.f;
    moveDesc.cct.groundLerpSpeed = 15.f;

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Movement"), _movementCom, &moveDesc), E_FAIL);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_MonsterStat"), _combatStat), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_SL_Mon_StateMachine"), _stateMachineCom), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Effect"), _effectCom), E_FAIL);

    _combatStat->Set_RuntimeCombatProfile(150.f, 0, 18.f, 0.f, 0.f);

    Bounding_OBB::BoundingOBBDesc bodyObbDesc{};
    bodyObbDesc.center = Vec3(0.f, 1.2f, 0.f);
    bodyObbDesc.extents = Vec3(0.65f, 1.2f, 0.45f);

    CHECK_FAILED(Ready_Collider(L"Com_Collider_OBB", &bodyObbDesc, Collision_Preset::SL_MonsterBody), E_FAIL);
    CHECK_FAILED(Rename_ComponentTag(TEXT("Com_Collider_OBB"), TEXT("Com_SL_MonsterBody")), E_FAIL);

    if (_movementCom)
        _movementCom->Set_Collider(_colliderCom);

    Bounding_OBB::BoundingOBBDesc attackObbDesc{};
    attackObbDesc.center = Vec3(0.f, 1.0f, 0.95f);
    attackObbDesc.extents = Vec3(0.55f, 0.65f, 0.85f);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_OBB"), _attackCom, &attackObbDesc), E_FAIL);
    CHECK_FAILED(Rename_ComponentTag(TEXT("Com_Collider_OBB"), TEXT("Com_SL_MonsterAttack")), E_FAIL);

    _attackCom->Set_CollisionPreset(Collision_Preset::SL_MonsterAttack);
    _attackCom->Set_IsActive(false);
    _attackCom->Clear_Overlap();

    return S_OK;
}

HRESULT SL_Dualliste::Ready_PartObjects()
{
    Mon_Dualliste_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Dualliste_Test";

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Dualliste_Body"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<Mon_Dualliste_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);

    if (Shared<ModelCom> model = _body->Get_Model())
        model->Refresh_AnimStateMappingFromOwnerContext();

    Mon_Dualliste_Sword_L::SWORD_DESC swordLDesc{};
    WeaponPrefabLoader::LoadedWeaponPrefab swordLPrefab{};
    if (!WeaponPrefabLoader::Load(Constants::Prefab::Dualliste_Sword_L, swordLPrefab, "Mon_Dualliste_Sword_L"))
        return E_FAIL;

    swordLDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    swordLDesc.modelType = swordLPrefab.modelType;
    swordLDesc.modelTag = swordLPrefab.modelTag;
    swordLDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("weapon_l");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Dualliste_Sword_L"), TEXT("Part_Sword_L"), &swordLDesc), E_FAIL);

    _swordL = dynamic_pointer_cast<Mon_Dualliste_Sword_L>(_partObjects[TEXT("Part_Sword_L")]);
    CHECK_NULL(_swordL, E_FAIL);
    _swordL->From_Json(swordLPrefab.rootJson);
    _swordL->Set_SourcePrefabName(Constants::Prefab::Dualliste_Sword_L);

    _attackLCom = _swordL->Get_AttackCollider();

    Mon_Dualliste_Sword_R::SWORD_DESC swordRDesc{};
    WeaponPrefabLoader::LoadedWeaponPrefab swordRPrefab{};
    if (!WeaponPrefabLoader::Load(Constants::Prefab::Dualliste_Sword_R, swordRPrefab, "Mon_Dualliste_Sword_R"))
        return E_FAIL;

    swordRDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    swordRDesc.modelType = swordRPrefab.modelType;
    swordRDesc.modelTag = swordRPrefab.modelTag;
    swordRDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("weapon_r");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Dualliste_Sword_R"), TEXT("Part_Sword_R"), &swordRDesc), E_FAIL);

    _swordR = dynamic_pointer_cast<Mon_Dualliste_Sword_R>(_partObjects[TEXT("Part_Sword_R")]);
    CHECK_NULL(_swordR, E_FAIL);
    _swordR->From_Json(swordRPrefab.rootJson);
    _swordR->Set_SourcePrefabName(Constants::Prefab::Dualliste_Sword_R);

    _attackRCom = _swordR->Get_AttackCollider();

    return S_OK;
}

Shared<SL_Dualliste> SL_Dualliste::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<SL_Dualliste>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : SL_Dualliste");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> SL_Dualliste::Clone(void* arg)
{
    auto clone = make_shared<SL_Dualliste>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void SL_Dualliste::Free()
{
    _cloneSkillManager.reset();
    SL_Base::Free();
}
