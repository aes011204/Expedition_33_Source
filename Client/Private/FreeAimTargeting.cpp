#include "FreeAimTargeting.h"
#include "BattleController.h"
#include "BattleStateMachine.h"
#include "BattleTurnCamera.h"
#include "ClientInstance.h"
#include "ColliderCom.h"
#include "CombatStatCom.h"
#include "GameInstance.h"
#include "PhysX_Types.h"
#include "GameObject.h"
#include "Character.h"
#include "ModelCom.h"

FreeAimTargeting::FreeAimTargeting(const Shared<BattleController>& owner)
    : Battle_State{ owner }
{
}

HRESULT FreeAimTargeting::Initialize()
{
    return Battle_State::Initialize();
}

void FreeAimTargeting::Begin_Play()
{
    Battle_State::Begin_Play();
}

bool FreeAimTargeting::EnterInvoke()
{
    Clear_HitTarget();
    _freeAimAnimState = FreeAimAnimState::None;

    auto owner = Get_Owner();
    if (!owner)
        return false;

    const auto battleCamera = owner->Get_BattleTurnCamera();
    if (battleCamera)
    {
        const Vec3 currentRot = battleCamera->Get_Transform()->Get_RotationEuler();
        battleCamera->Set_Yaw(currentRot.y);
        battleCamera->Set_Pitch(currentRot.x);
    }

    //GAME->Lock_Mouse();
    const BattleChar& actor = owner->Get_TurnAction()._curChar;
    if (actor.ModelCom == nullptr)
        return false;

    if (!actor.ModelCom->Set_AnimationByState(ETOI(BattleAnimType::FreeAim_Enter), 0.2f, false))
        return false;

    _freeAimAnimState = FreeAimAnimState::EnterTransition;

    return Battle_State::EnterInvoke();
}

void FreeAimTargeting::Update(float timeDelta)
{
    Battle_State::Update(timeDelta);

    auto owner = Get_Owner();
    auto battleFSM = Get_BattleFSM();

    if (!owner || !battleFSM)
        return;

     // Free Aim 발사 연출 중에는 조준 유지 상태만 두고 새 입력은 막는다.
    if (owner->Is_FreeAimShotPlaying())
        return;

    // 진입 중이고, 애니메이션이 끝나면
    if (_freeAimAnimState == FreeAimAnimState::EnterTransition && Is_CurrentActorAnimFinished())
    {
        // 애니메이션 끝난 뒤부터 실제 조준 자세 유지
        Play_CurrentActorBattleAnim(BattleAnimType::FreeAim_Idle, 0.15f, true);
        _freeAimAnimState = FreeAimAnimState::Idle;
    }

    if (const auto battleCamera = owner->Get_BattleTurnCamera())
        battleCamera->Update_FreeAimRotation(timeDelta); // Free Aim 상태에서만 카메라 회전 갱신

    Refresh_HitTarget(); // 회전된 카메라 방향 기준으로 적 히트 판정

    if (GAME->KeyDown(KEY_TYPE::RBUTTON))
    {
        Play_CurrentActorBattleAnim(BattleAnimType::Base_Idle, 0.2f, true); // 취소 시에는 기본 Idle로 전환
        Clear_HitTarget();
        battleFSM->Change_State(BattleTurnState::ChooseAction);
        return;
    }

    // 진입이 끝나기 전에는 Shoot 허용 X
    if (_freeAimAnimState != FreeAimAnimState::Idle)
        return;

    if (GAME->KeyDown(KEY_TYPE::LBUTTON))
    {
        Action shotAction{};
        shotAction._curChar = owner->Get_TurnAction()._curChar;
        shotAction.type = EBattleContext::FreeAimShoot;

        if (_hasCurrentHitTarget)
            shotAction.target = _currentHitTarget;
        else
            shotAction.target = {};

        owner->Begin_FreeAimShot(shotAction);
        return;
    }
}

void FreeAimTargeting::LateUpdate(float timeDelta)
{
    Battle_State::LateUpdate(timeDelta);
}

void FreeAimTargeting::ExitInvoke()
{
    //GAME->Unlock_Mouse();
    Clear_HitTarget();
    _freeAimAnimState = FreeAimAnimState::None;

    Battle_State::ExitInvoke();
}

bool FreeAimTargeting::Try_FindHitEnemy(BattleChar& outEnemy) const
{
    auto owner = Get_Owner();
    if (!owner)
        return false;

    const auto battleCamera = owner->Get_BattleTurnCamera();
    if (battleCamera == nullptr)
        return false;

    const Vec3 cameraOrigin = battleCamera->Get_AimOrigin();
    Vec3 direction = battleCamera->Get_AimDirection();
    if (direction.LengthSquared() <= 0.0001f)
        return false;

    direction.Normalize();

    constexpr float traceDistance = 100.f;
    constexpr float sweepRadius = 0.15f;

    PhysXQueryHit hit{};
    const bool hasHit = GAME->Sweep_Sphere(
        cameraOrigin,                  // 실제 판정은 카메라 위치 기준 그대로 유지
        direction,
        traceDistance,
        sweepRadius,
        Collision_Preset::Player_Target,
        hit);

    if (!hasHit || hit.owner == nullptr)
        return false;

    const vector<BattleChar> aliveEnemies = owner->Get_AliveEnemies();
    for (const auto& enemy : aliveEnemies)
    {
        if (enemy.actor == nullptr || enemy.statCom == nullptr)
            continue;

        if (enemy.actor.get() == hit.owner)
        {
            outEnemy = enemy;
            return true;
        }
    }

    return false;
}

void FreeAimTargeting::Refresh_HitTarget()
{
    BattleChar hitEnemy{};
    if (!Try_FindHitEnemy(hitEnemy))
    {
        Clear_HitTarget();
        return;
    }

    _currentHitTarget = hitEnemy;
    _hasCurrentHitTarget = true;

    if (auto owner = Get_Owner())
        owner->Set_TargetPreview(hitEnemy);

}

void FreeAimTargeting::Clear_HitTarget()
{
    _currentHitTarget = {};
    _hasCurrentHitTarget = false;

    if (auto owner = Get_Owner())
        owner->Clear_TargetPreview();
}

bool FreeAimTargeting::Is_CurrentActorAnimFinished() const
{
    auto owner = Get_Owner();
    if (!owner)
        return false;

    const BattleChar& actor = owner->Get_TurnAction()._curChar;
    if (actor.ModelCom == nullptr)
        return false;

    return actor.ModelCom->Is_AnimationFinished(); // 현재 재생 중인 진입 애니가 끝났는지 확인S
}

void FreeAimTargeting::Play_CurrentActorBattleAnim(BattleAnimType animType, float blendTime, bool isLoop) const
{
    auto owner = Get_Owner();
    if (!owner)
        return;

    const BattleChar& actor = owner->Get_TurnAction()._curChar;
    if (actor.ModelCom == nullptr)
        return;

    actor.ModelCom->Set_AnimationByState(ETOI(animType), blendTime, isLoop);
}

Shared<FreeAimTargeting> FreeAimTargeting::Create(const Shared<BattleController>& owner)
{
    auto instance = make_shared<FreeAimTargeting>(owner);

    if (FAILED(instance->Initialize()))
        return nullptr;

    return instance;
}

void FreeAimTargeting::Free()
{
    Battle_State::Free();
}
