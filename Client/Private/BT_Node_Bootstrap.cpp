#include"BT_Node_Bootstrap.h"
#include "BT_Attack.h"
#include "BT_Idle.h"
#include "BT_CheckDistance.h"
#include "BT_RotateToTarget.h"

NS_BEGIN(Client)

void Force_Register_ClientBTNodes()
{
    (void)BT_Attack::StaticClassNameA();
    (void)BT_Idle::StaticClassNameA();
    (void)BT_CheckDistance::StaticClassNameA();
    (void)BT_RotateToTarget::StaticClassNameA();
}

NS_END
