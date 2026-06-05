#include "AN_CounterReturnAll.h"

#include "BattleController.h"
#include "ClientInstance.h"

IMPLEMENT_REFLECTION(AN_CounterReturnAll)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    return true;
}

void AN_CounterReturnAll::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    if (context.model == nullptr)
        return;

    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    battle->StartCounterReturnAll(context.model);
}
