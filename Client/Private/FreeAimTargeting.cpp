#include "FreeAimTargeting.h"
#include <Battle_Event.h>
#include "BattleController.h"
#include "BattleStateMachine.h"
#include "BattleTurnCamera.h"
#include "ClientInstance.h"
#include "ColliderCom.h"
#include "CombatStatCom.h"
#include "GameInstance.h"
#include "PhysX_Types.h"
#include "GameObject.h"
#include "Monster_BattleBase.h"
#include "ModelCom.h"
#include "Event_Bus.h"
#include "Mon_WeakPoint.h"

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
    const BattleChar& actor = owner->Get_TurnActionRequest().actor;
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
    {
        battleCamera->Update_FreeAimRotation(timeDelta); // Free Aim 상태에서만 카메라 회전 갱신
        Update_FreeAimPlayerYaw(timeDelta);
    }
        

    Refresh_HitTarget(); // 회전된 카메라 방향 기준으로 적 히트 판정

    if (GAME->KeyDown(KEY_TYPE::RBUTTON))
    {
        Play_CurrentActorBattleAnim(BattleAnimType::Base_Idle, 0.2f, true); // 취소 시에는 기본 Idle로 전환
        Clear_HitTarget();

        EvtBattlePresentation event;
        event.cameraMode = BattleCameraMode::PLAYERFOCUS;
        event.subject = owner->Get_TurnActionRequest().actor.actor;
        GAME->Get_EventBus().Publish(event);

        battleFSM->Change_State(BattleTurnState::ChooseAction);
        return;
    }

    // 진입이 끝나기 전에는 Shoot 허용 X
    if (_freeAimAnimState != FreeAimAnimState::Idle)
        return;

    if (GAME->KeyDown(KEY_TYPE::LBUTTON))
    {
        ActionRequest shotRequest{};
        shotRequest.actor = owner->Get_TurnActionRequest().actor;
        shotRequest.type = EBattleContext::FreeAimShoot;

        if (_hasCurrentHitTarget)
            shotRequest.InitTarget = _currentHitTarget.target;
        else
            shotRequest.InitTarget = {};

        owner->ExecuteActionRequest(shotRequest);
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

bool FreeAimTargeting::Try_FindHitEnemy(BattleFreeAimHitTarget& outHitTarget) const
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
    constexpr float sweepRadius = 0.3f;

    const vector<BattleChar> aliveEnemies = owner->Get_AliveEnemies();
    const auto resolveHitTarget = [&](const PhysXQueryHit& hit, BattleFreeAimHitTarget& outResolvedHitTarget) -> bool
    {
        if (hit.owner == nullptr)
            return false;

        for (const auto& enemy : aliveEnemies)
        {
            if (enemy.actor == nullptr || enemy.statCom == nullptr)
                continue;

            if (enemy.actor.get() == hit.owner)
            {
                outResolvedHitTarget.target = enemy;
                outResolvedHitTarget.hitPart = FreeAimHitPart::Body;
                return true;
            }

            const Shared<Monster_BattleBase> monster = dynamic_pointer_cast<Monster_BattleBase>(enemy.actor);
            if (monster == nullptr)
                continue;

            const Shared<Mon_WeakPoint> weakPoint = monster->Get_BattleWeakPoint();
            if (weakPoint == nullptr || weakPoint.get() != hit.owner)
                continue;

            if (!weakPoint->Is_BattleWeakPointEnabled())
                return false;

            outResolvedHitTarget.target = enemy;
            outResolvedHitTarget.hitPart = FreeAimHitPart::WeakPoint;
            return true;
        }

        return false;
    };

    PhysXQueryHit hit{};
    const bool hasHit = GAME->Sweep_Sphere(
        cameraOrigin,
        direction,
        traceDistance,
        sweepRadius,
        Collision_Preset::Player_Target,
        hit);

    if (!hasHit || hit.owner == nullptr)
        return false;

    return resolveHitTarget(hit, outHitTarget);
}

void FreeAimTargeting::Refresh_HitTarget()
{
    BattleFreeAimHitTarget  hitTarget{};
    if (!Try_FindHitEnemy(hitTarget))
    {
        Clear_HitTarget();
        return;
    }

    _currentHitTarget = hitTarget;
    _hasCurrentHitTarget = true;

    if (auto owner = Get_Owner())
    {
        owner->Set_FreeAimHitTarget(hitTarget);
    }
}

void FreeAimTargeting::Clear_HitTarget()
{
    _currentHitTarget = {};
    _hasCurrentHitTarget = false;

    if (auto owner = Get_Owner())
        owner->Clear_FreeAimHitTarget();
}

bool FreeAimTargeting::Is_CurrentActorAnimFinished() const
{
    auto owner = Get_Owner();
    if (!owner)
        return false;

    const BattleChar& actor = owner->Get_TurnActionRequest().actor;
    if (actor.ModelCom == nullptr)
        return false;

    return actor.ModelCom->Is_AnimationFinished(); // 현재 재생 중인 진입 애니가 끝났는지 확인S
}

void FreeAimTargeting::Play_CurrentActorBattleAnim(BattleAnimType animType, float blendTime, bool isLoop) const
{
    auto owner = Get_Owner();
    if (!owner)
        return;

    const BattleChar& actor = owner->Get_TurnActionRequest().actor;
    if (actor.ModelCom == nullptr)
        return;

    actor.ModelCom->Set_AnimationByState(ETOI(animType), blendTime, isLoop);
}

void FreeAimTargeting::Update_FreeAimPlayerYaw(float timeDelta)
{
    auto owner = Get_Owner();
    if (!owner)
        return;

    const auto battleCamera = owner->Get_BattleTurnCamera();
    if (!battleCamera)
        return;

    const BattleChar& actor = owner->Get_TurnActionRequest().actor;
    if (actor.actor == nullptr)
        return;

    const auto transform = actor.actor->Get_Transform();
    if (!transform)
        return;

    Vec3 aimDir = battleCamera->Get_AimDirection();
    aimDir.y = 0.f;

    aimDir.Normalize();

    // 회전 속도
    constexpr float yawSpeedDegPerSec = 720.f;

    const float targetYaw = XMConvertToDegrees(atan2f(aimDir.x, aimDir.z));

    Vec3 rot = transform->Get_RotationEuler();
    float deltaYaw = targetYaw - rot.y;

    while (deltaYaw > 180.f)
        deltaYaw -= 360.f;

    while (deltaYaw < -180.f)
        deltaYaw += 360.f;

    const float maxStep = yawSpeedDegPerSec * timeDelta;
    deltaYaw = ::clamp(deltaYaw, -maxStep, maxStep);

    rot.y += deltaYaw;
    transform->Set_RotationEuler(rot);
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
