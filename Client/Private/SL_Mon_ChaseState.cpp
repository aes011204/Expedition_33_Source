#include "SL_Mon_ChaseState.h"

#include "SL_Dualliste.h"

NS_BEGIN(Client)

SL_Mon_ChaseState::SL_Mon_ChaseState(const Shared<SL_Dualliste>& owner)
    : SL_Mon_State(owner)
{
}

bool SL_Mon_ChaseState::EnterInvoke()
{
    if (!SL_Mon_State::EnterInvoke())
        return false;

    const Shared<SL_Dualliste> owner = _owner.lock();
    if (owner == nullptr)
        return false;

    return owner->Play_Anim(SL_Dualliste::Anim::Run, 0.1f, true);
}

Shared<SL_Mon_ChaseState> SL_Mon_ChaseState::Create(const Shared<SL_Dualliste>& owner)
{
    auto instance = make_shared<SL_Mon_ChaseState>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[SL_MON_CHASE] : Failed to Create State");
        return nullptr;
    }

    return instance;
}

NS_END
