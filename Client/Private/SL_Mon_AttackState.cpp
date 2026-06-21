#include "SL_Mon_AttackState.h"

#include "SL_Dualliste.h"
#include "SL_Mon_StateMachineCom.h"

NS_BEGIN(Client)

SL_Mon_AttackState::SL_Mon_AttackState(const Shared<SL_Dualliste>& owner)
    : SL_Mon_State(owner)
{
}

bool SL_Mon_AttackState::EnterInvoke()
{
    if (!SL_Mon_State::EnterInvoke())
        return false;

    Shared<SL_Dualliste> owner = _owner.lock();
    if (owner == nullptr)
        return false;

    return owner->Play_Anim(SL_Dualliste::Anim::Attack_Combo_1, 0.1f, false);
}

void SL_Mon_AttackState::Update(float timeDelta)
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

Shared<SL_Mon_AttackState> SL_Mon_AttackState::Create(const Shared<SL_Dualliste>& owner)
{
    auto instance = make_shared<SL_Mon_AttackState>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[SL_MON_ATTACK] : Failed to Create State");
        return nullptr;
    }

    return instance;
}

NS_END
