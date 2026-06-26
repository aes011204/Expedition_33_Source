#include "AN_SL_JumpCounterDamage.h"

#include "ClientInstance.h"
#include "SoulLikeController.h"

IMPLEMENT_REFLECTION(AN_SL_JumpCounterDamage)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "AN_SL_JumpCounterDamage";
    info.category = "SoulLike";

    return true;
}

void AN_SL_JumpCounterDamage::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    if (const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController())
        controller->Apply_JumpCounterDamage();
}
