#include "AN_SL_CachePlayerPosition.h"

#include "ClientInstance.h"
#include "ContainerObject.h"
#include "GameObject.h"
#include "SL_Dualliste.h"
#include "SL_Maelle.h"
#include "SL_Mon_AttackState.h"
#include "SL_Mon_StateMachineCom.h"
#include "SoulLikeController.h"
#include "TransformCom.h"

IMPLEMENT_REFLECTION(AN_SL_CachePlayerPosition)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "AN_SL_CachePlayerPosition";
    info.category = "SoulLike";

    return true;
}

void AN_SL_CachePlayerPosition::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    const Shared<SL_Dualliste> owner = Resolve_OwnerDualliste(context.owner);
    if (owner == nullptr)
        return;

    const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController();
    const Shared<SL_Maelle> player = controller ? controller->Get_Player() : nullptr;
    const Shared<TransformCom> playerTransform = player ? player->Get_Transform() : nullptr;
    if (playerTransform == nullptr)
        return;

    const Shared<SL_Mon_StateMachineCom> stateMachine = owner->Get_Component<SL_Mon_StateMachineCom>();
    const Shared<SL_Mon_AttackState> attackState = stateMachine
        ? dynamic_pointer_cast<SL_Mon_AttackState>(stateMachine->Get_CurrentMonState())
        : nullptr;
    if (attackState == nullptr)
        return;

    attackState->Cache_PlayerAnchor(
        playerTransform->Get_WorldPosition(),
        playerTransform->Get_WorldRight(),
        playerTransform->Get_WorldUp(),
        playerTransform->Get_WorldForward());
}

Shared<SL_Dualliste> AN_SL_CachePlayerPosition::Resolve_OwnerDualliste(GameObject* notifyOwner) const
{
    Shared<GameObject> current = notifyOwner ? notifyOwner->GetSharedPtr<GameObject>() : nullptr;

    while (current)
    {
        if (const Shared<SL_Dualliste> owner = dynamic_pointer_cast<SL_Dualliste>(current))
            return owner;

        if (dynamic_pointer_cast<ContainerObject>(current) && current->Get_Owner() == nullptr)
            return dynamic_pointer_cast<SL_Dualliste>(current);

        current = current->Get_Owner();
    }

    return nullptr;
}
