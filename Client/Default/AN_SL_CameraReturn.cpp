#include "AN_SL_CameraReturn.h"

#include "ClientInstance.h"
#include "SoulLikeController.h"

IMPLEMENT_REFLECTION(AN_SL_CameraReturn)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "AN_SL_CameraReturn";
    info.category = "SoulLike";

    return true;
}

void AN_SL_CameraReturn::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController();
    if (controller == nullptr)
        return;

    controller->End_BattleShotCamera();
}
