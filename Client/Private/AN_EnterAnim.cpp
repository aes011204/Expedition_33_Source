#include "AN_EnterAnim.h"

#include <ModelCom.h>
#include "BattleController.h"
#include "ClientInstance.h"

IMPLEMENT_REFLECTION(AN_EnterAnim)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_ENUM("_animType", _animType, BattleAnimType);


    return true;
}

void AN_EnterAnim::Execute(const AnimNotifyContext& context)
{
    CLIENT->Get_BattleController()->End_FirstAnim();
    context.model->Set_AnimationByState(ETOI(_animType), 0.3f, true);
}
