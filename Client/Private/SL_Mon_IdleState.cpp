#include "SL_Mon_IdleState.h"

#include "SL_Dualliste.h"

NS_BEGIN(Client)

SL_Mon_IdleState::SL_Mon_IdleState(const Shared<SL_Dualliste>& owner)
    : SL_Mon_State(owner)
{
}

bool SL_Mon_IdleState::EnterInvoke()
{
    if (!SL_Mon_State::EnterInvoke())
        return false;

    Shared<SL_Dualliste> owner = _owner.lock();
    if (owner == nullptr)
        return false;

    return owner->Play_Anim(SL_Dualliste::Anim::Idle, 0.15f, true);
}

Shared<SL_Mon_IdleState> SL_Mon_IdleState::Create(const Shared<SL_Dualliste>& owner)
{
    auto instance = make_shared<SL_Mon_IdleState>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[SL_MON_IDLE] : Failed to Create State");
        return nullptr;
    }

    return instance;
}

NS_END
