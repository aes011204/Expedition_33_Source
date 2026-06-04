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

    //CLIENT->Get_BattleController()->End_FirstAnim();
    ///
    const auto Battle = CLIENT->Get_BattleController();
    if (Battle->Is_BattleStarted() == false)
        return;

    const BattleAction& action = Battle->Get_TurnAction();

    if (dynamic_pointer_cast<GameObject>(action.Get_CurChar().actor) != context.owner->Get_Owner())
        return;

    if (action.Get_CurChar().statCom->Get_CurAliveState() == EAliveStatus::Dead)
        return;
    ///

    context.model->Set_AnimationByState(ETOI(_animType), 0.3f, true);
}
