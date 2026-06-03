#include "pch.h"
#include "AN_WorldFreeAimHit.h"

#include "PlayableParts.h"
#include "Player_FreeAim.h"
#include "PlayerStateMachineCom.h"
#include "Player_WorldBase.h"

IMPLEMENT_REFLECTION(AN_WorldFreeAimHit)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();


    return true;
}

void AN_WorldFreeAimHit::Execute(const AnimNotifyContext& context)
{
    if (context.owner == nullptr)
        return;

    Shared<PlayableParts> part = context.owner->GetSharedPtr<PlayableParts>();
    if (part == nullptr)
        return;

    Shared<Player_WorldBase> player = dynamic_pointer_cast<Player_WorldBase>(part->Get_Owner());
    if (player == nullptr)
        return;

    Shared<PlayerStateMachineCom> stateCom = player->Get_Component<PlayerStateMachineCom>();
    if (stateCom == nullptr)
        return;

    Shared<State> state = stateCom->Get_State(PlayerState::FreeAim);
    Shared<Player_FreeAim> freeAimState = dynamic_pointer_cast<Player_FreeAim>(state);
    if (freeAimState == nullptr)
        return;

    freeAimState->Trigger_PendingFreeAimShotHit();
}
