#include "ANS_ParryWindow.h"

#include "GameObject.h"
#include "SL_Maelle.h"

IMPLEMENT_REFLECTION(ANS_ParryWindow)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "ANS_ParryWindow";
    info.category = "SoulLike";

    return true;
}

void ANS_ParryWindow::On_Begin(const AnimNotifyContext& context)
{
    if (context.isPreview || context.isCinematic)
        return;

    if (Shared<SL_Maelle> maelle = Resolve_Maelle(context.owner))
        maelle->Open_ParryWindow();
}

void ANS_ParryWindow::On_Tick(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);
}

void ANS_ParryWindow::On_End(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    if (Shared<SL_Maelle> maelle = Resolve_Maelle(context.owner))
        maelle->Close_ParryWindow();
}

Shared<SL_Maelle> ANS_ParryWindow::Resolve_Maelle(GameObject* notifyOwner) const
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
