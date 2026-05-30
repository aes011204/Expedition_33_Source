#include "AN_Battle_EndAnim.h"

#include <ModelCom.h>

#include "BattleController.h"
#include "ClientInstance.h"

IMPLEMENT_REFLECTION(AN_Battle_EndAnim)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_STRING("테스트 값", _nxtAnimName);

    return true;
}

void AN_Battle_EndAnim::Execute(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    if (!context.owner || !context.model)
        return;

    const auto Battle = CLIENT->Get_BattleController();
    if (Battle->Is_BattleStarted() == false)
        return;

    const BattleAction& action = Battle->Get_TurnAction();

    if (dynamic_pointer_cast<GameObject>(action.Get_CurChar().actor) != context.owner->Get_Owner())
        return;

    if (action.Get_CurChar().ModelCom.get() != context.model)
        return;



    Battle->OnActionEndNotify();
}
