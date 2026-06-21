#include "SL_Mon_StunState.h"

#include "AIController.h"
#include "BlackBoard.h"
#include "SL_Dualliste.h"
#include "SL_Mon_StateMachineCom.h"

NS_BEGIN(Client)

SL_Mon_StunState::SL_Mon_StunState(const Shared<SL_Dualliste>& owner)
    : SL_Mon_State(owner)
{
}

bool SL_Mon_StunState::EnterInvoke()
{
    if (!SL_Mon_State::EnterInvoke())
        return false;

    _phase = Phase::Enter;

    return Play_PhaseAnim(_phase);
}

void SL_Mon_StunState::Update(float timeDelta)
{
    SL_Mon_State::Update(timeDelta);

    const Shared<SL_Dualliste> owner = _owner.lock();
    if (owner == nullptr || _stateCom == nullptr)
        return;

    if (owner->Is_Dead())
    {
        _stateCom->Change_State(SL_MonState::Dead);
        return;
    }

    switch (_phase)
    {
    case Phase::Enter:
        if (owner->Is_AnimFinished())
        {
            _phase = Phase::Loop;
            Play_PhaseAnim(_phase);
        }
        break;

    case Phase::Loop:
        if (!Is_StunActive())
        {
            _phase = Phase::Exit;
            Play_PhaseAnim(_phase);
        }
        break;

    case Phase::Exit:
        if (owner->Is_AnimFinished())
            _stateCom->Change_State(SL_MonState::Idle);
        break;
    }
}

bool SL_Mon_StunState::Is_StunActive() const
{
    const Shared<SL_Dualliste> owner = _owner.lock();
    if (owner == nullptr)
        return false;

    const Shared<AIController> aiCtrl = owner->Get_Component<AIController>();
    if (aiCtrl == nullptr)
        return false;

    const Shared<BlackBoard> bb = aiCtrl->Get_BlackBoard();
    if (bb == nullptr)
        return false;

    const any* value = bb->Find_Value("IsStun");
    if (value == nullptr)
        return false;

    return any_cast<bool>(*value);
}

bool SL_Mon_StunState::Play_PhaseAnim(Phase phase)
{
    const Shared<SL_Dualliste> owner = _owner.lock();
    if (owner == nullptr)
        return false;

    switch (phase)
    {
    case Phase::Enter:
        return owner->Play_Anim(SL_Dualliste::Anim::Stun_Start, 0.1f, false);

    case Phase::Loop:
        return owner->Play_Anim(SL_Dualliste::Anim::Stun_Idle, 0.1f, true);

    case Phase::Exit:
        return owner->Play_Anim(SL_Dualliste::Anim::Stun_End, 0.1f, false);
    }

    return false;
}

Shared<SL_Mon_StunState> SL_Mon_StunState::Create(const Shared<SL_Dualliste>& owner)
{
    auto instance = make_shared<SL_Mon_StunState>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[SL_MON_STUN] : Failed to Create State");
        return nullptr;
    }

    return instance;
}

NS_END
