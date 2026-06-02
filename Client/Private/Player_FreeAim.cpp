#include "pch.h"
#include "Player_FreeAim.h"

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

namespace
{
    constexpr float FREE_AIM_TRACE_DISTANCE = 200.f;
    constexpr float FREE_AIM_MONSTER_SWEEP_RADIUS = 0.3f;
    constexpr float FREE_AIM_HIT_EFFECT_NORMAL_OFFSET = 0.05f;

    const wstring   WORLD_FREE_AIM_HIT_EFFECT_NAME = L"World_FreeAim_Hit.effect.json";
}

Player_FreeAim::Player_FreeAim(const Shared<Player_WorldBase>& owner)
    : Player_State{ owner } {}

HRESULT Player_FreeAim::Initialize()
{
    return Player_State::Initialize();
}

void Player_FreeAim::Begin_Play()
{
    Player_State::Begin_Play();

    if (auto model = _bodyPlayer->Get_Model())
    {
        for (auto& modifier : model->Get_Modifiers())
        {
            if (auto offsetMod = dynamic_pointer_cast<OffsetAnimationModifier>(modifier))
            {
                const wstring& tag = modifier->Get_ModifierTag();

                if (tag == L"AimOffset") {
                    _offsetModifier = offsetMod;
                }
                else if (tag == L"Walk_F") _walkModifiers[0] = offsetMod;
                else if (tag == L"Walk_B") _walkModifiers[1] = offsetMod;
                else if (tag == L"Walk_L") _walkModifiers[2] = offsetMod;
                else if (tag == L"Walk_R") _walkModifiers[3] = offsetMod;
                else if (tag == L"Walk_LB") _walkModifiers[4] = offsetMod;
                else if (tag == L"Walk_RB") _walkModifiers[5] = offsetMod;
            }
        }
    }
}

using plAnim = Body_Player::PlayerWorldAnimation;

bool Player_FreeAim::EnterInvoke()
{
    _bodyPlayer->Set_AnimationByState(ETOI(plAnim::Pl_FreeAim_Idle), 0.1f, true);
    if (_offsetModifier) _offsetModifier->Set_Active(true);

    _hasHitMonster = false;
    _hitMonster.reset();

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

    return true;
}

void Player_FreeAim::Update(float timeDelta)
{
    if (false == IsEnterFreeAim())
    {
        _hasHitMonster = false;
        _hitMonster.reset();

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
        if (Shared<Monster_WorldBase> hitMonster = _hitMonster.lock())
        {
            // TODO : Hit Monster 로그. 여기서 몬스터 상태전환해서 Stun으로 만들면 됩니다.
            LOG_DEBUG("World FreeAim Shoot Hit Monster: {}", String::ToString(hitMonster->Get_Name()).c_str());

            Play_FreeAimHitEffect();

            hitMonster->Apply_Stun(3.f);
        }

        if (animIndex != ETOI(plAnim::Pl_FreeAim_Shoot))
        {
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
    _hasHitMonster = false;
    _hitMonster.reset();

    if (_offsetModifier)
    {
        _offsetModifier->Set_Active(false);
    }

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

    Shared<Monster_WorldBase> hitMonster = Find_FreeAimHitMonster(&hitInfo);

    _hasHitMonster = (hitMonster != nullptr);
    _hitMonster = hitMonster;

    _currentHitInfo = _hasHitMonster ? hitInfo : FreeAimHitInfo{};
}

Shared<Monster_WorldBase> Player_FreeAim::Find_FreeAimHitMonster(FreeAimHitInfo* outHitInfo) const
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

    PhysXQueryHit monsterHit{};
    const bool hasMonsterHit = GAME->Sweep_Sphere(
        queryOrigin,
        direction,
        FREE_AIM_TRACE_DISTANCE,
        FREE_AIM_MONSTER_SWEEP_RADIUS,
        Collision_Preset::Player_Target,
        monsterHit);

    if (!hasMonsterHit || monsterHit.owner == nullptr)
        return nullptr;

    Monster_WorldBase* hitMonster = dynamic_cast<Monster_WorldBase*>(monsterHit.owner);
    if (hitMonster == nullptr)
        return nullptr;

    if (hasEnvBlock && envHit.distance < monsterHit.distance)
        return nullptr;

    Shared<Monster_WorldBase> hitMonsterShared = hitMonster->GetSharedPtr<Monster_WorldBase>();

    if (outHitInfo != nullptr)
    {
        outHitInfo->monster = hitMonsterShared;
        outHitInfo->hitPosition = monsterHit.position;
        outHitInfo->hitNormal = monsterHit.normal;
    }

    return hitMonsterShared;
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
    CLIENT->Play_Effect(effectDesc);
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
