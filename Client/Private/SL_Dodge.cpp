#include "SL_Dodge.h"

#include "Body_Player.h"
#include "Camera.h"
#include "GameInstance.h"
#include "SL_Maelle.h"
#include "SL_StateMachineCom.h"
#include "TransformCom.h"

SL_Dodge::SL_Dodge(const Shared<SL_Maelle>& owner)
    : SL_State(owner)
{
}

bool SL_Dodge::EnterInvoke()
{
    Shared<SL_Maelle> owner = _owner.lock();
    if (owner == nullptr)
        return false;

    owner->Set_CombatLocked(true);
    owner->Set_InvincibleTime(0.35f); // 구르기 시 무적 시간 세팅

    Stop_Movement();

    if (!_request.lockOn)
    {
        Shared<Camera> camera = GAME->Get_ActiveCamera();
        Shared<TransformCom> transform = owner->Get_Transform();

        if (camera != nullptr && transform != nullptr)
        {
            Vec3 cameraForward = Vec3::Zero;
            Vec3 cameraRight = Vec3::Zero;

            if (camera->Try_GetFlatMoveBasis(cameraForward, cameraRight))
            {
                cameraForward.y = 0.f;

                if (cameraForward.LengthSquared() > 0.0001f)
                {
                    cameraForward.Normalize();

                    const Vec3 position = transform->Get_WorldPosition();
                    transform->LookAt(position + cameraForward);
                }
            }
        }
    }

    _direction = Resolve_DodgeDirection(_request);
    _anim = Resolve_DodgeAnim(_direction);
    
    Play_Anim(_anim, 0.05f, false);

    return true;
}

void SL_Dodge::Update(float timeDelta)
{
    if (!Is_AnimFinished())
        return;

    Stop_Movement();

    if (Shared<SL_Maelle> owner = _owner.lock())
        owner->Set_CombatLocked(false);

    if (_stateCom == nullptr)
        return;

    if (_request.worldDir.LengthSquared() > 0.f)
    {
        if (_stateCom->Change_State(SL_PlayerState::Run))
            _stateCom->Request_Move(_request, timeDelta);

        return;
    }

    _stateCom->Change_State(SL_PlayerState::Idle);
}

void SL_Dodge::ExitInvoke()
{
    Stop_Movement();

    if (Shared<SL_Maelle> owner = _owner.lock())
        owner->Set_CombatLocked(false);

    SL_State::ExitInvoke();
}

bool SL_Dodge::Request_Move(const SL_MoveRequest& request, float timeDelta)
{
    UNREFERENCED_PARAMETER(request);
    UNREFERENCED_PARAMETER(timeDelta);

    return true;
}

SL_DodgeDirection SL_Dodge::Resolve_DodgeDirection(const SL_MoveRequest& request) const
{
    const float moveX = request.moveX;
    const float moveZ = request.moveZ;

    if (fabsf(moveX) <= 0.001f && fabsf(moveZ) <= 0.001f)
        return SL_DodgeDirection::Fwd;

    if (fabsf(moveZ) >= fabsf(moveX))
        return moveZ < 0.f ? SL_DodgeDirection::Back : SL_DodgeDirection::Fwd;

    return moveX < 0.f ? SL_DodgeDirection::Left : SL_DodgeDirection::Right;
}

SoulLikeAnim SL_Dodge::Resolve_DodgeAnim(SL_DodgeDirection direction) const
{
    switch (direction)
    {
    case SL_DodgeDirection::Back:
        return SoulLikeAnim::Dodge_Back;

    case SL_DodgeDirection::Left:
        return SoulLikeAnim::Dodge_Left;

    case SL_DodgeDirection::Right:
        return SoulLikeAnim::Dodge_Right;

    case SL_DodgeDirection::Fwd:
    default:
        return SoulLikeAnim::Dodge_Fwd;
    }
}

Shared<SL_Dodge> SL_Dodge::Create(const Shared<SL_Maelle>& owner)
{
    auto instance = make_shared<SL_Dodge>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[SL_DODGE] : Failed to Create State");
        return nullptr;
    }

    return instance;
}
