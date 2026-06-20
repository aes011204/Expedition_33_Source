#include "SL_Move.h"

#include "Body_Player.h"
#include "Character.h"
#include "SL_Maelle.h"
#include "SL_StateMachineCom.h"
#include "SL_TargetCom.h"
#include "TransformCom.h"

SL_Move::SL_Move(const Shared<SL_Maelle>& owner)
    : SL_State(owner)
{
}

bool SL_Move::EnterInvoke()
{
    return SL_State::EnterInvoke();
}

bool SL_Move::Request_Move(const SL_MoveRequest& request, float timeDelta)
{
    if (_stateCom == nullptr)
        return false;

    Shared<SL_Maelle> owner = _owner.lock();
    if (owner == nullptr || owner->Is_CombatLocked())
        return false;

    if (request.worldDir.LengthSquared() <= 0.f)
    {
        _stateCom->Change_State(SL_PlayerState::Idle);
        return true;
    }

    owner->Apply_MoveInput(request, timeDelta);

    // 타겟팅
    if (request.lockOn)
    {
        Shared<SL_TargetCom> targetCom = owner->Get_Component<SL_TargetCom>();
        Shared<Character> target = targetCom ? targetCom->Get_LockedTarget() : nullptr;

        if (target)
        {
            Vec3 lookPos = target->Get_Transform()->Get_WorldPosition();
            lookPos.y = owner->Get_Transform()->Get_WorldPosition().y;

            owner->Get_Transform()->LookAt(lookPos);
        }
    }

    SoulLikeAnim moveAnim = SoulLikeAnim::Run;

    if (request.lockOn)
    {
        const float absMoveX = fabsf(request.moveX);
        const float absMoveZ = fabsf(request.moveZ);

        if (absMoveZ >= absMoveX)
            moveAnim = request.moveZ >= 0.f ? SoulLikeAnim::Target_Run_Fwd : SoulLikeAnim::Target_Run_Back;
        else
            moveAnim = request.moveX >= 0.f ? SoulLikeAnim::Target_Run_Right : SoulLikeAnim::Target_Run_Left;
    }

    if (_body != nullptr && _body->Get_AnimationStateIndex() != ETOI(moveAnim))
        Play_Anim(moveAnim, 0.15f, true);

    return true;
}

bool SL_Move::Request_BasicAttack()
{
    return _stateCom != nullptr && _stateCom->Change_State(SL_PlayerState::BasicAttack);
}

bool SL_Move::Request_Dodge(const SL_MoveRequest& request)
{
    return _stateCom != nullptr && _stateCom->Change_State(SL_PlayerState::Dodge);
}

bool SL_Move::Request_Parry()
{
    return _stateCom != nullptr && _stateCom->Change_State(SL_PlayerState::Parry);
}

bool SL_Move::Request_Skill(const Skill_Data& skillData, uint32 slotIndex)
{
    CHECK_NULL(_stateCom , false);

    return _stateCom->Change_State(SL_PlayerState::Skill);
}

bool SL_Move::Request_Jump()
{
    CHECK_NULL(_stateCom, false);

    return _stateCom->Change_State(SL_PlayerState::Jump);
}

Shared<SL_Move> SL_Move::Create(const Shared<SL_Maelle>& owner)
{
    auto instance = make_shared<SL_Move>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[SL_MOVE] : Failed to Create State");
        return nullptr;
    }

    return instance;
}
