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
    if (Battle && Battle->Is_BattleStarted() == false)
        return;

    BattleChar notifyOwnerChar = Battle->Get_TurnAction().Get_CurChar();
    if (notifyOwnerChar.actor == nullptr)
        notifyOwnerChar = Battle->Get_TurnActionRequest().actor;

    if (dynamic_pointer_cast<GameObject>(notifyOwnerChar.actor) != context.owner->Get_Owner())
        return;

    if (notifyOwnerChar.statCom == nullptr ||
        notifyOwnerChar.statCom->Get_CurAliveState() == EAliveStatus::Dead)
        return;

    context.model->Set_AnimationByState(ETOI(_animType), 0.3f, true);
}
