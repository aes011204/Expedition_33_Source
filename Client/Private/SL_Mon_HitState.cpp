#include "SL_Mon_HitState.h"

#include "SL_Dualliste.h"
#include "SL_Mon_StateMachineCom.h"

NS_BEGIN(Client)

SL_Mon_HitState::SL_Mon_HitState(const Shared<SL_Dualliste>& owner)
    : SL_Mon_State(owner)
{
}

bool SL_Mon_HitState::EnterInvoke()
{
    if (!SL_Mon_State::EnterInvoke())
        return false;

    Shared<SL_Dualliste> owner = _owner.lock();
    if (owner == nullptr)
        return false;

    return owner->Play_Anim(SL_Dualliste::Anim::Hit, 0.1f, false);
}

void SL_Mon_HitState::Update(float timeDelta)
{
    SL_Mon_State::Update(timeDelta);

    Shared<SL_Dualliste> owner = _owner.lock();
    if (owner == nullptr || _stateCom == nullptr)
        return;

    if (owner->Is_Dead())
    {
        _stateCom->Change_State(SL_MonState::Dead);
        return;
    }

    if (!owner->Is_AnimFinished())
        return;

    _stateCom->Change_State(SL_MonState::Idle);
}

Shared<SL_Mon_HitState> SL_Mon_HitState::Create(const Shared<SL_Dualliste>& owner)
{
    auto instance = make_shared<SL_Mon_HitState>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[SL_MON_HIT] : Failed to Create State");
        return nullptr;
    }

    return instance;
}

NS_END
