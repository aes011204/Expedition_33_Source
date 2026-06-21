#include "ANS_SL_JumpCounterWindow.h"

#include "ClientInstance.h"
#include "SoulLikeController.h"

IMPLEMENT_REFLECTION(ANS_SL_JumpCounterWindow)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "ANS_SL_JumpCounterWindow";
    info.category = "SoulLike";

    return true;
}

void ANS_SL_JumpCounterWindow::On_Begin(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    if (const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController())
        controller->Set_JumpCounterWindowOpen(true);
}

void ANS_SL_JumpCounterWindow::On_Tick(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);
}

void ANS_SL_JumpCounterWindow::On_End(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    if (const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController())
        controller->Set_JumpCounterWindowOpen(false);
}
