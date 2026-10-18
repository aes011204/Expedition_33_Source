#include "SL_Idle.h"
#include "SL_Move.h"
#include "SL_StateMachineCom.h"

SL_Idle::SL_Idle(const Shared<SL_Maelle>& owner)
    : SL_State(owner)
{
}

bool SL_Idle::EnterInvoke()
{
    Stop_Movement();

    Play_Anim(SoulLikeAnim::Idle, 0.15f, true);

    return true;
}

void SL_Idle::Update(float timeDelta)
{
    SL_State::Update(timeDelta);
}

bool SL_Idle::Request_Move(const SL_MoveRequest& request, float timeDelta)
{
    if (_stateCom == nullptr)
        return false;

    if (request.worldDir.LengthSquared() <= 0.f)
        return false;

    if (auto moveState = dynamic_pointer_cast<SL_Move>(_stateCom->Get_State(SL_PlayerState::Run)))
        moveState->Set_PlayIdleToRun(true);

    if (!_stateCom->Change_State(SL_PlayerState::Run))
        return false;

    return _stateCom->Request_Move(request, timeDelta);
}

bool SL_Idle::Request_BasicAttack()
{
    return _stateCom != nullptr && _stateCom->Change_State(SL_PlayerState::BasicAttack);
}

bool SL_Idle::Request_Dodge(const SL_MoveRequest& request)
{
    return _stateCom != nullptr && _stateCom->Change_State(SL_PlayerState::Dodge);
}

bool SL_Idle::Request_Parry()
{
    return _stateCom != nullptr && _stateCom->Change_State(SL_PlayerState::Parry);
}

bool SL_Idle::Request_Skill(const Skill_Data& skillData, uint32 slotIndex)
{
    CHECK_NULL(_stateCom, false);

    return _stateCom->Change_State(SL_PlayerState::Skill);
}

bool SL_Idle::Request_Jump()
{
    CHECK_NULL(_stateCom, false);

    return _stateCom->Change_State(SL_PlayerState::Jump);
}

Shared<SL_Idle> SL_Idle::Create(const Shared<SL_Maelle>& owner)
{
    auto instance = make_shared<SL_Idle>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[SL_IDLE] : Failed to Create State");
        return nullptr;
    }

    return instance;
}
