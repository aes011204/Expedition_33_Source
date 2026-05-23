#include"BT_Node_Bootstrap.h"
#include "BT_Chase.h"
#include "BT_Idle.h"
#include "BT_CheckDistance.h"
#include "BT_FinishDecision.h"
#include "BT_RotateToTarget.h"
#include "BT_SetSkill.h"
#include "BT_Set_Target.h"

NS_BEGIN(Client)

void Force_Register_ClientBTNodes()
{
    (void)BT_Chase::StaticClassNameA();
    (void)BT_Idle::StaticClassNameA();
    (void)BT_CheckDistance::StaticClassNameA();
    (void)BT_RotateToTarget::StaticClassNameA();
    (void)BT_FinishDecision::StaticClassNameA();
    (void)BT_Set_Target::StaticClassNameA();
    (void)BT_SetSkill::StaticClassNameA();
}

NS_END
