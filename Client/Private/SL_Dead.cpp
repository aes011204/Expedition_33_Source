#include "SL_Dead.h"
#include "SL_Maelle.h"

SL_Dead::SL_Dead(const Shared<SL_Maelle>& owner)
    : SL_State(owner)
{
}

bool SL_Dead::EnterInvoke()
{
    Shared<SL_Maelle> owner = _owner.lock();
    if (owner == nullptr)
        return false;

    owner->Set_CombatLocked(true);
    Stop_Movement();

    Play_Anim(SoulLikeAnim::Dead, 0.1f, false);

    // 죽으면 다시 월드로 돌아가게? 이건 컨트롤러에서 처리해야할듯

    return true;
}

Shared<SL_Dead> SL_Dead::Create(const Shared<SL_Maelle>& owner)
{
    auto instance = make_shared<SL_Dead>(owner);

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("[SL_DEAD] : Failed to Create State");
        return nullptr;
    }

    return instance;
}
