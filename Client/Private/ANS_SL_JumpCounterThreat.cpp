#include "ANS_SL_JumpCounterThreat.h"

#include "ClientInstance.h"
#include "GameObject.h"
#include "SL_Base.h"
#include "SoulLikeController.h"

IMPLEMENT_REFLECTION(ANS_SL_JumpCounterThreat)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "ANS_SL_JumpCounterThreat";
    info.category = "SoulLike";

    return true;
}

void ANS_SL_JumpCounterThreat::On_Begin(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController();
    if (controller == nullptr)
        return;

    controller->Set_JumpCounterThreat(Resolve_Attacker(context.owner), true);
}

void ANS_SL_JumpCounterThreat::On_Tick(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);
}

void ANS_SL_JumpCounterThreat::On_End(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController();
    if (controller == nullptr)
        return;

    controller->Set_JumpCounterThreat(Resolve_Attacker(context.owner), false);
}

Shared<SL_Base> ANS_SL_JumpCounterThreat::Resolve_Attacker(GameObject* notifyOwner) const
{
    Shared<GameObject> current = notifyOwner ? notifyOwner->GetSharedPtr<GameObject>() : nullptr;

    while (current)
    {
        if (Shared<SL_Base> attacker = dynamic_pointer_cast<SL_Base>(current))
            return attacker;

        current = current->Get_Owner();
    }

    return nullptr;
}
