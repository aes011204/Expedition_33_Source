#include "AN_EnterAnim.h"

#include <ModelCom.h>
#include "BattleController.h"
#include "ClientInstance.h"


void AN_EnterAnim::Execute(const AnimNotifyContext& context)
{
    CLIENT->Get_BattleController()->End_FirstAnim();
    context.model->Set_AnimationByState(ETOI(BattleAnimType::Base_Idle), 0.3f, true);
}
