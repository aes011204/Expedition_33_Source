#include "AN_CanFreeAimShoot.h"

#include "BattleController.h"
#include "ClientInstance.h"

IMPLEMENT_REFLECTION(AN_CanFreeAimShoot)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    return true;
}

void AN_CanFreeAimShoot::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview || context.isCinematic)
        return;

    const auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr || !battle->Is_BattleStarted())
        return;

    battle->Notify_CanFreeAimShoot();
}
