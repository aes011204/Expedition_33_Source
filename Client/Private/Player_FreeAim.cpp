#include "pch.h"
#include "Player_FreeAim.h"
#include "ColliderCom.h"
#include "Camera.h"
#include "OffsetAnimationModifier.h"
#include "Body_Player.h"
#include "Monster_WorldBase.h"
#include "PhysX_Types.h"
#include "PlayerControllerCom.h"
#include "PlayerStateMachineCom.h"
#include "Player_WorldBase.h"
#include "TargetCamera.h"
#include "ClientInstance.h"
#include "Client_Constants.h"
#include "EyeTarget.h"

namespace
{
    constexpr float FREE_AIM_TRACE_DISTANCE = 800.f;
    constexpr float FREE_AIM_MONSTER_SWEEP_RADIUS = 0.3f;
    constexpr float FREE_AIM_HIT_EFFECT_NORMAL_OFFSET = 0.05f;
    const wstring WORLD_FREE_AIM_HIT_EFFECT_NAME = L"World_FreeAim_Hit_EX.effect.json";
    
    Vec3 MakeLookAlignedRotationDegree(Vec3 direction)
    {
        if (direction.LengthSquared() <= 0.0001f)
            direction = Vec3::Look;
        else
            direction.Normalize();

        const Quat rotation = Math::CreateRotationBetweenVectors(Vec3::Look, direction);
        const Vec3 eulerRadian = rotation.ToEuler();

        return Vec3(
            XMConvertToDegrees(eulerRadian.x),
            XMConvertToDegrees(eulerRadian.y),
            XMConvertToDegrees(eulerRadian.z));
    }
}

Player_FreeAim::Player_FreeAim(const Shared<Player_WorldBase>& owner)
    : Player_State{ owner } {
}

HRESULT Player_FreeAim::Initialize()
{
    return Player_State::Initialize();
}

void Player_FreeAim::Begin_Play()
{
    Player_State::Begin_Play();
}

void Player_FreeAim::Refresh_BodyPlayer()
{
    Player_State::Refresh_BodyPlayer();
    Bind_AnimationModifiers();
}

void Player_FreeAim::Bind_AnimationModifiers()
{
    _offsetModifier.reset();
    for (auto& walkModifier : _walkModifiers)
        walkModifier.reset();

    if (_bodyPlayer == nullptr)
        return;

    if (_bodyPlayer == nullptr)
        return;

    if (auto model = _bodyPlayer->Get_Model())
    {
        for (auto& modifier : model->Get_Modifiers())
        {
            if (auto offsetMod = dynamic_pointer_cast<OffsetAnimationModifier>(modifier))
            {
                const wstring& tag = modifier->Get_ModifierTag();

                if (tag == L"AimOffset")
                    _offsetModifier = offsetMod;
                else if (tag == L"Walk_F")
                    _walkModifiers[0] = offsetMod;
                else if (tag == L"Walk_B")
                    _walkModifiers[1] = offsetMod;
                else if (tag == L"Walk_L")
                    _walkModifiers[2] = offsetMod;
                else if (tag == L"Walk_R")
                    _walkModifiers[3] = offsetMod;
                else if (tag == L"Walk_LB")
                    _walkModifiers[4] = offsetMod;
                else if (tag == L"Walk_RB")
                    _walkModifiers[5] = offsetMod;
            }
        }
    }
}

using plAnim = Body_Player::PlayerWorldAnimation;

bool Player_FreeAim::EnterInvoke()
{
    Refresh_BodyPlayer();
    if (_bodyPlayer == nullptr || _controllerCom == nullptr || _stateCom == nullptr)
        return false;

    _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_FreeAim_Idle), 0.1f, true);
    if (_offsetModifier)
        _offsetModifier->Set_Active(true);

    _hasHitObject = false;
    _hitObject.reset();
    Clear_PendingFreeAimShot(); // 이전 발사 잔여 상태를 들고 들어오지 않도록 초기화한다.

    auto owner = _owner.lock();
    if (owner)
    {
        auto movementCom = owner->Get_Component<MovementCom>();
        if (movementCom)
        {
            const auto& moveDesc = movementCom->Get_MoveDesc();

            _prevTurnSpeedDegree = moveDesc.turnSpeedDegree;
            _prevTurnResponse = moveDesc.turnResponse;

            movementCom->Set_TurnSpeedDegree(_freeAimTurnSpeedDegree);
            movementCom->Set_TurnResponse(_freeAimTurnResponse);

            _isEnterTurnBurstActive = true;
        }
    }

    GAME->Play_SoundCue(Constants::SoundCue::SFX_FreeAimStart);

    return true;
}

void Player_FreeAim::Update(float timeDelta)
{
    if (_bodyPlayer == nullptr || _controllerCom == nullptr || _stateCom == nullptr)
    {
        if (_stateCom != nullptr)
            _stateCom->Change_State(PlayerState::Idle);
        return;
    }

    if (false == IsEnterFreeAim())
    {
        _hasHitObject = false;
        _hitObject.reset();
        Clear_PendingFreeAimShot();

        if (IsEnterMove())
        {
            _stateCom->Change_State(PlayerState::Move);
            return;
        }
        else
        {
            _stateCom->Change_State(PlayerState::Idle);
            return;
        }
    }

    Update_EnterTurnBurst();
    Update_FreeAimHitState();

    uint32 animIndex = _bodyPlayer->Get_AnimationStateIndex();

    if (IsFreeAimShoot())
    {
        // 입력 순간의 조준 결과를 고정해 두고, 실제 히트는 notify 프레임까지 미룬다.
        if (animIndex != ETOI(plAnim::Pl_FreeAim_Shoot))
        {
            Cache_PendingFreeAimShot();

            GAME->Play_SoundCue(Constants::SoundCue::SFX_FreeAimHit);
            _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_FreeAim_Shoot), 0.25f, false);
            _bodyPlayer->Set_AnimationSpeed(2.f);
        }
    }
    else
    {
        if (animIndex == ETOI(plAnim::Pl_FreeAim_Shoot) && _bodyPlayer->Is_AnimationFinished())
        {
            _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_FreeAim_Idle), 0.25f, true);
            _bodyPlayer->Set_AnimationSpeed(1.f);
            Clear_PendingFreeAimShot(); // 발사가 끝났으면 다음 샷을 위해 pending 상태를 비운다.
        }
    }

    Update_DirectionalModifiers();

    if (_offsetModifier)
    {
        if (auto camera = GAME->Get_ActiveCamera())
        {
            _offsetModifier->Set_AimPitch(camera->Get_Pitch());

            auto rigid = _controllerCom->Get_RigidControl();
            rigid.rigidLookDir = camera->Get_Transform()->Get_WorldForward();
            rigid.rigidWalk = true;
            _controllerCom->Set_RigidControl(rigid);
        }
    }
}

void Player_FreeAim::LateUpdate(float timeDelta)
{
}

void Player_FreeAim::ExitInvoke()
{
    GAME->Play_SoundCue(Constants::SoundCue::SFX_FreeAimStop);

    _hasHitObject = false;
    _hitObject.reset();
    Clear_PendingFreeAimShot();

    if (_bodyPlayer != nullptr)
        _bodyPlayer->Set_AnimationSpeed(1.f); // shoot 2배속 상태로 FreeAim을 빠져나가도 다음 상태에 속도를 남기지 않는다.

    if (_offsetModifier)
        _offsetModifier->Set_Active(false);

    for (int i = 0; i < 6; ++i)
    {
        if (_walkModifiers[i])
            _walkModifiers[i]->Set_Active(false);
    }

    _isEnterTurnBurstActive = false;
}

void Player_FreeAim::Update_FreeAimHitState()
{
    FreeAimHitInfo hitInfo{};

    Shared<GameObject> hitMonster = Find_FreeAimHitTarget(&hitInfo);

    _hasHitObject = (hitMonster != nullptr);
    _hitObject = hitMonster;

    _currentHitInfo = _hasHitObject ? hitInfo : FreeAimHitInfo{};
}

void Player_FreeAim::Cache_PendingFreeAimShot()
{
    FreeAimHitInfo hitInfo{};
    Shared<GameObject> hitMonster = Find_FreeAimHitTarget(&hitInfo);

    _hasPendingShotHit = (hitMonster != nullptr);
    _hasConsumedPendingShotHit = false;
    _pendingShotObject = hitMonster;
    _pendingShotHitInfo = _hasPendingShotHit ? hitInfo : FreeAimHitInfo{};
}

void Player_FreeAim::Clear_PendingFreeAimShot()
{
    _hasPendingShotHit = false;
    _hasConsumedPendingShotHit = false;
    _pendingShotObject.reset();
    _pendingShotHitInfo = FreeAimHitInfo{};
}

void Player_FreeAim::Trigger_PendingFreeAimShotHit()
{
    if (_hasConsumedPendingShotHit)
        return; // 같은 발사 애니메이션에서 두 번 맞지 않게 막는다.

    if (_stateCom == nullptr || _stateCom->Get_CurrentPlayerState() != PlayerState::FreeAim)
        return; // FreeAim 상태가 끝난 뒤 도착한 notify는 이전 발사 결과를 소비하지 않는다.

    if (_bodyPlayer == nullptr)
        return;

    if (_bodyPlayer->Get_AnimationStateIndex() != ETOI(plAnim::Pl_FreeAim_Shoot))
        return; // FreeAim_Shoot 중이 아닐 때 들어온 notify는 무시한다.

    _hasConsumedPendingShotHit = true;

    if (!_hasPendingShotHit)
        return; // 발사 시작 시점에 맞춘 대상이 없었으면 히트 없이 지나간다.

    Shared<GameObject> hitObject = _pendingShotObject.lock();

    if (!hitObject)
        return;

    _currentHitInfo = _pendingShotHitInfo; // 히트 이펙트는 발사 시점에 캡처한 위치/노멀을 사용한다.

    Play_FreeAimHitEffect();

    if(Shared<Monster_WorldBase> hitMonster = dynamic_pointer_cast<Monster_WorldBase>(hitObject))
    {
        hitMonster->Apply_Stun(3.f);
        LOG_DEBUG("World FreeAim Shoot Hit Monster: {}", String::ToString(hitMonster->Get_Name()).c_str());
    }
    else if(Shared<EyeTarget> eyeTarget = dynamic_pointer_cast<EyeTarget>(hitObject))
    {
       eyeTarget->Try_Consume_EyeTargetHit();
    }

    CLIENT->Notify_FreeAimShotHit();
}

void Player_FreeAim::Update_DirectionalModifiers()
{
    MovementCom::MovementInput input = _controllerCom->Get_MovementInput();

    int targetWalkIndex = -1; // 0:F, 1:B, 2:LF, 3:RF, 4:LB, 5:RB

    float moveX = input.moveX;
    float moveZ = input.moveZ;

    if (abs(moveZ) > 0.1f || abs(moveX) > 0.1f)
    {
        if (moveZ < -0.1f && abs(moveX) > 0.1f) {
            targetWalkIndex = (moveX < 0.f) ? 4 : 5; // LB(4), RB(5)
        }
        else if (abs(moveZ) > abs(moveX) * 1.5f) {
            targetWalkIndex = (moveZ > 0.f) ? 0 : 1;
        }
        else {
            targetWalkIndex = (moveX < 0.f) ? 2 : 3; // LF(2), RF(3)
        }
    }

    for (int i = 0; i < 6; ++i)
    {
        if (_walkModifiers[i])
            _walkModifiers[i]->Set_Active(i == targetWalkIndex);
    }
}

void Player_FreeAim::Update_EnterTurnBurst()
{
    if (!_isEnterTurnBurstActive)
        return;

    auto owner = _owner.lock();
    if (!owner)
        return;

    auto movementCom = owner->Get_Component<MovementCom>();
    if (!movementCom)
        return;

    auto camera = GAME->Get_ActiveCamera();
    if (!camera)
        return;

    auto ownerTransform = owner->Get_Transform();
    auto cameraTransform = camera->Get_Transform();
    if (!ownerTransform || !cameraTransform)
        return;

    Vec3 actorForward = ownerTransform->Get_WorldForward();
    Vec3 aimForward = cameraTransform->Get_WorldForward();

    actorForward.y = 0.f;
    aimForward.y = 0.f;

    if (actorForward.LengthSquared() <= 0.f || aimForward.LengthSquared() <= 0.f)
        return;

    actorForward.Normalize();
    aimForward.Normalize();

    const float dot = std::clamp(actorForward.Dot(aimForward), -1.f, 1.f);

    if (dot >= 0.995f)
    {
        movementCom->Set_TurnSpeedDegree(_prevTurnSpeedDegree);
        movementCom->Set_TurnResponse(_prevTurnResponse);
        _isEnterTurnBurstActive = false;
    }
}

Shared<GameObject> Player_FreeAim::Find_FreeAimHitTarget(FreeAimHitInfo* outHitInfo) const
{
    const Shared<Camera> activeCamera = GAME->Get_ActiveCamera();
    if (activeCamera == nullptr)
        return nullptr;

    const Shared<TargetCamera> targetCamera = dynamic_pointer_cast<TargetCamera>(activeCamera);
    if (targetCamera == nullptr)
        return nullptr;

    const Vec3 queryOrigin = targetCamera->Get_FreeAimQueryOrigin();
    Vec3 direction = targetCamera->Get_FreeAimQueryDirection();

    if (direction.LengthSquared() <= 0.0001f)
        return nullptr;

    direction.Normalize();

    PhysXQueryHit envHit{};
    const bool hasEnvBlock = GAME->Sweep_Sphere(
        queryOrigin,
        direction,
        FREE_AIM_TRACE_DISTANCE,
        FREE_AIM_MONSTER_SWEEP_RADIUS,
        Collision_Preset::Camera,
        envHit);

    PhysXQueryHit objectHit{};
    const bool hasObjectHit = GAME->Sweep_Sphere(
        queryOrigin,
        direction,
        FREE_AIM_TRACE_DISTANCE,
        FREE_AIM_MONSTER_SWEEP_RADIUS,
        Collision_Preset::Player_Target,
        objectHit);

    if (!hasObjectHit || objectHit.collider == nullptr)
        return nullptr;

    const Shared<GameObject> hitOwner = objectHit.collider->Get_Owner();
    if (!hitOwner)
        return nullptr;

    if (hasEnvBlock && envHit.distance < objectHit.distance)
        return nullptr;

    Shared<GameObject> hitObjectShared = dynamic_pointer_cast<GameObject>(hitOwner);
    if (!hitObjectShared && hitOwner->Get_Owner())
        hitObjectShared = dynamic_pointer_cast<GameObject>(hitOwner->Get_Owner());

    if (!hitObjectShared)
        return nullptr;

    if (outHitInfo != nullptr)
    {
        outHitInfo->hitPosition = objectHit.position;
        outHitInfo->shotDirection = direction;
        outHitInfo->hitNormal = objectHit.normal;
    }

    return hitObjectShared;
}

void Player_FreeAim::Play_FreeAimHitEffect() const
{
    Vec3 hitNormal = _currentHitInfo.hitNormal;
    if (hitNormal.LengthSquared() > 0.0001f)
        hitNormal.Normalize();
    else
        hitNormal = Vec3::Up;

    const Vec3 effectPosition =
        _currentHitInfo.hitPosition + hitNormal * FREE_AIM_HIT_EFFECT_NORMAL_OFFSET;

    PlayEffectDesc effectDesc{};
    effectDesc.effectName = WORLD_FREE_AIM_HIT_EFFECT_NAME;
    effectDesc.worldPosition = effectPosition;
    effectDesc.localRotation = MakeLookAlignedRotationDegree(_currentHitInfo.shotDirection);
    CLIENT->Play_Effect(effectDesc);
}

Shared<Player_FreeAim> Player_FreeAim::Create(const Shared<Player_WorldBase>& owner)
{
    auto instance = make_shared<Player_FreeAim>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[PLAYER FREE AIM] : Failed to Create State");
        return nullptr;
    }

    return instance;
}

void Player_FreeAim::Free()
{
    Player_State::Free();
}
