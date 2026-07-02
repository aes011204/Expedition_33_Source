#include "ANS_ParkourCameraLag.h"

#include "GameInstance.h"
#include "GameObject.h"
#include "Player_WorldBase.h"
#include "TargetCamera.h"
#include "TransformCom.h"

IMPLEMENT_REFLECTION(ANS_ParkourCameraLag)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "ANS_ParkourCameraLag";
    info.category = "World";

    return true;
}

void ANS_ParkourCameraLag::On_Begin(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    const Shared<Player_WorldBase> player = Resolve_Player(context.owner);
    if (player == nullptr)
        return;

    const Shared<TransformCom> transform = player->Get_Transform();
    if (transform == nullptr)
        return;

    if (const Shared<TargetCamera> targetCamera =
        dynamic_pointer_cast<TargetCamera>(GAME->Get_CameraByType(ObjectType::TargetCamera)))
    {
        targetCamera->Begin_ParkourVerticalLag(transform);
    }
}

void ANS_ParkourCameraLag::On_Tick(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);
}

void ANS_ParkourCameraLag::On_End(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    const Shared<Player_WorldBase> player = Resolve_Player(context.owner);
    if (player == nullptr)
        return;

    const Shared<TransformCom> transform = player->Get_Transform();
    if (transform == nullptr)
        return;

    if (const Shared<TargetCamera> targetCamera =
        dynamic_pointer_cast<TargetCamera>(GAME->Get_CameraByType(ObjectType::TargetCamera)))
    {
        targetCamera->End_ParkourVerticalLag(transform);
    }
}

Shared<Player_WorldBase> ANS_ParkourCameraLag::Resolve_Player(GameObject* notifyOwner) const
{
    Shared<GameObject> current = notifyOwner ? notifyOwner->GetSharedPtr<GameObject>() : nullptr;

    while (current)
    {
        if (Shared<Player_WorldBase> player = dynamic_pointer_cast<Player_WorldBase>(current))
            return player;

        current = current->Get_Owner();
    }

    return nullptr;
}
