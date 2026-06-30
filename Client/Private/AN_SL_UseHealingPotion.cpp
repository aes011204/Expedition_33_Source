#include "AN_SL_UseHealingPotion.h"

#include "ClientInstance.h"
#include "GameObject.h"
#include "SL_Maelle.h"
#include "SoulLikeController.h"

IMPLEMENT_REFLECTION(AN_SL_UseHealingPotion)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "AN_SL_UseHealingPotion";
    info.category = "SoulLike";

    return true;
}

void AN_SL_UseHealingPotion::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    const Shared<SL_Maelle> maelle = Resolve_Maelle(context.owner);
    if (maelle == nullptr)
        return;

    const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController();
    if (controller == nullptr)
        return;

    controller->Try_ApplyHealingPotion(maelle);
}

Shared<SL_Maelle> AN_SL_UseHealingPotion::Resolve_Maelle(GameObject* notifyOwner) const
{
    Shared<GameObject> current = notifyOwner ? notifyOwner->GetSharedPtr<GameObject>() : nullptr;

    while (current)
    {
        if (Shared<SL_Maelle> maelle = dynamic_pointer_cast<SL_Maelle>(current))
            return maelle;

        current = current->Get_Owner();
    }

    return nullptr;
}
