#include "SL_Mon_DeadState.h"

#include "SL_Dualliste.h"

NS_BEGIN(Client)

SL_Mon_DeadState::SL_Mon_DeadState(const Shared<SL_Dualliste>& owner)
    : SL_Mon_State(owner)
{
}

bool SL_Mon_DeadState::EnterInvoke()
{
    if (!SL_Mon_State::EnterInvoke())
        return false;

    Shared<SL_Dualliste> owner = _owner.lock();
    if (owner == nullptr)
        return false;

    return owner->Play_Anim(SL_Dualliste::Anim::Dead, 0.1f, false);
}

void SL_Mon_DeadState::Update(float timeDelta)
{
    SL_Mon_State::Update(timeDelta);
}

Shared<SL_Mon_DeadState> SL_Mon_DeadState::Create(const Shared<SL_Dualliste>& owner)
{
    auto instance = make_shared<SL_Mon_DeadState>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[SL_MON_DEAD] : Failed to Create State");
        return nullptr;
    }

    return instance;
}

NS_END
