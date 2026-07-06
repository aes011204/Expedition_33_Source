#include "AN_CounterDamage.h"

#include "BattleController.h"
#include "ClientInstance.h"

IMPLEMENT_REFLECTION(AN_CounterDamage)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    
    return true;
}

void AN_CounterDamage::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview || context.isCinematic)
        return;

    if (!context.owner)
        return;

    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    battle->Get_ReactionController().ApplyCounterDamage(context.model);
}
