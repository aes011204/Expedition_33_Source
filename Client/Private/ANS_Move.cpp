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
    _originPos = action.originPos;
}

void ANS_Move::On_Tick(const AnimNotifyContext& context)
{
    //TODO:: 애님 노티파이 스테이트의 프로그레스 진행도 필요함
    auto action =  CLIENT->Get_BattleController()->Get_TurnAction();
    auto _curProgress = action._curChar.ModelCom->Get_CurrentProgress();

    action._curChar.actor->Get_Transform()->Set_Position(Vec3::Lerp(_startPos, _originPos, _curProgress));
   
}

void ANS_Move::On_End(const AnimNotifyContext& context)
{
}
