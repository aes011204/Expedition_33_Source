#include "ANS_Move.h"

#include <GameObject.h>
#include <ModelCom.h>
#include "BattleController.h"
#include "ClientInstance.h"
#include "Character_BattleBase.h"

void ANS_Move::On_Begin(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    auto& action = battle->Get_TurnAction();
    if (!action.Get_CurChar().actor)
        return;

    _startPos = action.Get_CurChar().actor->Get_Transform()->Get_Position();
    _goalPos = action.GetPhaseGoalPos();
}

void ANS_Move::On_Tick(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    auto& action = battle->Get_TurnAction();
    if (!action.Get_CurChar().actor)
        return;

    float _curProgress = context.notifyStateProgress;

    action.Get_CurChar().actor->Get_Transform()->Set_Position(Vec3::Lerp(_startPos, _goalPos, _curProgress));

}

void ANS_Move::On_End(const AnimNotifyContext& context)
{
    if (context.isPreview)
        return;

    if (context.owner == nullptr)
        return;

    auto battle = CLIENT->Get_BattleController();
    if (battle == nullptr)
        return;

    auto& action = battle->Get_TurnAction();

    if (!action.Get_CurChar().actor)
        return;

    action.Get_CurChar().actor->Get_Transform()->Set_Position(_goalPos);
}
