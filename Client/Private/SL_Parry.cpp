#include "SL_Parry.h"

#include "SL_Maelle.h"
#include "SL_StateMachineCom.h"
#include "GameInstance.h"

SL_Parry::SL_Parry(const Shared<SL_Maelle>& owner)
    : SL_State(owner)
{
}

bool SL_Parry::EnterInvoke()
{
    Shared<SL_Maelle> owner = _owner.lock();
    if (owner == nullptr)
        return false;

    owner->Set_CombatLocked(true);
    owner->Reset_ParryConsumed();
    owner->Close_ParryWindow(); // 성공 판정은 애니메이션 notify state가 여는 구간에서만 허용한다.

    Stop_Movement();
    Play_Anim(Resolve_ParryAnim(), 0.05f, false);

    return true;
}

void SL_Parry::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!Is_AnimFinished())
        return;

    if (Shared<SL_Maelle> owner = _owner.lock())
    {
        owner->Close_ParryWindow();
        owner->Set_CombatLocked(false);
    }

    if (_stateCom)
        _stateCom->Change_State(SL_PlayerState::Idle);
}

void SL_Parry::ExitInvoke()
{
    if (Shared<SL_Maelle> owner = _owner.lock())
    {
        owner->Close_ParryWindow();
        owner->Set_CombatLocked(false);
    }

    SL_State::ExitInvoke();
}

SoulLikeAnim SL_Parry::Resolve_ParryAnim() const
{
    return GAME->Random(0.f, 1.f) < 0.5f ? SoulLikeAnim::Parry : SoulLikeAnim::Parry2;
}

Shared<SL_Parry> SL_Parry::Create(const Shared<SL_Maelle>& owner)
{
    auto instance = make_shared<SL_Parry>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[SL_PARRY] : Failed to Create State");
        return nullptr;
    }

    return instance;
}
