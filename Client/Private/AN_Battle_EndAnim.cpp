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

    auto Battle = CLIENT->Get_BattleController();
    if (Battle->Is_BattleStarted() == false)
        return;
    Battle->OnActionEndNotify();
}

