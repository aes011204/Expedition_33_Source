#include "AN_JumpCounterStart.h"

#include "BattleController.h"
#include "ClientInstance.h"

IMPLEMENT_REFLECTION(AN_JumpCounterStart)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    return true;
}

void AN_JumpCounterStart::Execute(const AnimNotifyContext& context)
{
    if (!context.owner)
        return;

    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    battle->Get_ReactionController().StartCounter(context.model);
}
