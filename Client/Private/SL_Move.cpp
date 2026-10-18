#include "SL_Move.h"

#include "Body_Player.h"
#include "Character.h"
#include "SL_Maelle.h"
#include "SL_StateMachineCom.h"
#include "TransformCom.h"

SL_Move::SL_Move(const Shared<SL_Maelle>& owner)
    : SL_State(owner)
{
}

bool SL_Move::EnterInvoke()
{
    if (!SL_State::EnterInvoke())
        return false;

    _idleToRunPlaying = false;
    _runToIdlePlaying = false;

    if (_playIdleToRun)
    {
        _playIdleToRun = false;
        _idleToRunPlaying = Play_Anim(SoulLikeAnim::Idle_To_Run, 0.05f, false);
    }

    return true;
}

void SL_Move::Update(float timeDelta)
{
    SL_State::Update(timeDelta);

    if (_runToIdlePlaying && Is_AnimFinished())
    {
        _runToIdlePlaying = false;

        if (_stateCom != nullptr)
            _stateCom->Change_State(SL_PlayerState::Idle);
    }
}

void SL_Move::ExitInvoke()
{
    _playIdleToRun = false;
    _idleToRunPlaying = false;
    _runToIdlePlaying = false;

    SL_State::ExitInvoke();
}

void SL_Move::Set_PlayIdleToRun(bool play)
{
    _playIdleToRun = play;
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
        if (!_runToIdlePlaying)
        {
            _idleToRunPlaying = false;
            _runToIdlePlaying = Play_Anim(SoulLikeAnim::Run_To_Idle, 0.08f, false);
            Stop_Movement();
        }

        return true;
    }

    if (_runToIdlePlaying)
        _runToIdlePlaying = false;

    owner->Apply_MoveInput(request, timeDelta);

    if (_idleToRunPlaying)
    {
        if (!Is_AnimFinished())
            return true;

        _idleToRunPlaying = false;
    }

    SoulLikeAnim moveAnim = SoulLikeAnim::Run;

    if (request.lockOn && !request.sprint)
    {
        const float moveX = request.moveX;
        const float moveZ = request.moveZ;
        const float absMoveX = fabsf(moveX);
        const float absMoveZ = fabsf(moveZ);

        if (absMoveX <= 0.001f)
        {
            moveAnim = moveZ >= 0.f ? SoulLikeAnim::TargetWalk_F : SoulLikeAnim::TargetWalk_B;
        }
        else if (absMoveZ <= 0.001f)
        {
            moveAnim = moveX >= 0.f ? SoulLikeAnim::TargetWalk_FR90 : SoulLikeAnim::TargetWalk_FL90;
        }
        else if (moveZ >= 0.f)
        {
            if (moveX < 0.f)
                moveAnim = absMoveX > absMoveZ ? SoulLikeAnim::TargetWalk_FL90 : SoulLikeAnim::TargetWalk_FL45;
            else
                moveAnim = absMoveX > absMoveZ ? SoulLikeAnim::TargetWalk_FR90 : SoulLikeAnim::TargetWalk_FR45;
        }
        else
        {
            if (moveX < 0.f)
                moveAnim = absMoveX > absMoveZ ? SoulLikeAnim::TargetWalk_BL90 : SoulLikeAnim::TargetWalk_BL45;
            else
                moveAnim = absMoveX > absMoveZ ? SoulLikeAnim::TargetWalk_BR90 : SoulLikeAnim::TargetWalk_BR45;
        }
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
