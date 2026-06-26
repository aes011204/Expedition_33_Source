#include "ANS_SL_MonsterAttackCamera.h"

#include "ClientInstance.h"
#include "ContainerObject.h"
#include "GameObject.h"
#include "SL_Base.h"
#include "SoulLikeController.h"

IMPLEMENT_REFLECTION(ANS_SL_MonsterAttackCamera)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "ANS_SL_MonsterAttackCamera";
    info.category = "SoulLike";

    PROPERTY_STRING("Preset Key", _presetKey);
    PROPERTY_BOOL("Restore On End", _restoreOnEnd);

    return true;
}

void ANS_SL_MonsterAttackCamera::On_Begin(const AnimNotifyContext& context)
{
    if (context.isPreview || context.isCinematic)
        return;

    const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController();
    if (controller == nullptr)
        return;

    controller->Begin_MonsterAttackCameraPreset(Resolve_Attacker(context.owner), _presetKey);
}

void ANS_SL_MonsterAttackCamera::On_Tick(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);
}

void ANS_SL_MonsterAttackCamera::On_End(const AnimNotifyContext& context)
{
    if (context.isPreview || context.isCinematic || !_restoreOnEnd)
        return;

    const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController();
    if (controller == nullptr)
        return;

    controller->End_MonsterAttackCameraPreset(Resolve_Attacker(context.owner));
}

Shared<SL_Base> ANS_SL_MonsterAttackCamera::Resolve_Attacker(GameObject* notifyOwner) const
{
    Shared<GameObject> current = notifyOwner ? notifyOwner->GetSharedPtr<GameObject>() : nullptr;

    while (current)
    {
        if (const Shared<SL_Base> attacker = dynamic_pointer_cast<SL_Base>(current))
            return attacker;

        if (dynamic_pointer_cast<ContainerObject>(current) && current->Get_Owner() == nullptr)
            return dynamic_pointer_cast<SL_Base>(current);

        current = current->Get_Owner();
    }

    return nullptr;
}
