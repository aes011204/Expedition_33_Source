#include "ANS_Move.h"

#include <GameObject.h>
#include <ModelCom.h>
#include "BattleController.h"
#include "ClientInstance.h"
#include "Character_BattleBase.h"

void ANS_Move::On_Begin(const AnimNotifyContext& context)
{
    auto& action = CLIENT->Get_BattleController()->Get_TurnAction();
   
    _startPos = action._curChar.actor->Get_Transform()->Get_Position();
    _goalPos = action.phaseGoalPos;
}

void ANS_Move::On_Tick(const AnimNotifyContext& context)
{
    auto& action =  CLIENT->Get_BattleController()->Get_TurnAction();
    float _curProgress = context.notifyStateProgress;

    action._curChar.actor->Get_Transform()->Set_Position(Vec3::Lerp(_startPos, _goalPos, _curProgress));

    LOG_DEBUG("prograss{}", _curProgress);
    LOG_DEBUG("PosZ{}", action._curChar.actor->Get_Transform()->Get_Position().z);

}

void ANS_Move::On_End(const AnimNotifyContext& context)
{
    auto& action = CLIENT->Get_BattleController()->Get_TurnAction();
    action._curChar.actor->Get_Transform()->Set_Position(Vec3::Lerp(_startPos, _goalPos, 1));

    LOG_DEBUG("PosZ{}", action._curChar.actor->Get_Transform()->Get_Position().z);


}
