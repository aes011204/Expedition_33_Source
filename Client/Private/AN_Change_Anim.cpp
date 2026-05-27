#include "AN_Change_Anim.h"

#include <ModelCom.h>
#include "BattleController.h"
#include "ClientInstance.h"

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

    context.model->Set_AnimationByState(ETOI(_animType), 0.3f, true);
}
