#include "AN_StartCounter.h"

#include "BattleController.h"
#include "ClientInstance.h"

IMPLEMENT_REFLECTION(AN_StartCounter)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    return true;
}

void AN_StartCounter::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview || context.isCinematic)
        return;

    if (!context.owner)
        return;

    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    battle->Get_ReactionController().StartCounter(context.model);
}
