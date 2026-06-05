#include "AN_Change_Anim.h"

#include <ModelCom.h>
#include "BattleController.h"
#include "ClientInstance.h"
#include "CombatStatCom.h"

IMPLEMENT_REFLECTION(AN_Change_Anim)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_ENUM("_animType", _animType, BattleAnimType);


    return true;
}

void AN_Change_Anim::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    if (context.model == nullptr)
        return;

    const auto Battle = CLIENT->Get_BattleController();
    if (Battle == nullptr || Battle->Is_BattleStarted() == false)
        return;

    const Shared<GameObject> notifyOwner = context.owner ? context.owner->Get_Owner() : nullptr;
    if (notifyOwner == nullptr)
        return;

    const BattleAction& action = Battle->Get_TurnAction();
    const ActionRequest& request = Battle->Get_TurnActionRequest();
    const BattleChar* notifyActor = nullptr;

    if (action.Get_CurChar().actor &&
        dynamic_pointer_cast<GameObject>(action.Get_CurChar().actor) == notifyOwner)
    {
        notifyActor = &action.Get_CurChar();
    }
    else if (request.actor.actor &&
        dynamic_pointer_cast<GameObject>(request.actor.actor) == notifyOwner)
    {
        notifyActor = &request.actor;
    }
    else
    {
        return;
    }

    if (notifyActor->statCom == nullptr ||
        notifyActor->statCom->Get_CurAliveState() == EAliveStatus::Dead)
        return;

    context.model->Set_AnimationByState(ETOI(_animType), 0.3f, true);
}
