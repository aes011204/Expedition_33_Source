#include "BattleAction.h"

#include "BattleController.h"
#include "ClientInstance.h"
#include "CombatStatCom.h"

void BattleAction::NextStep()
{
    _curStepIndex++;

    if (_curStepIndex >= _steps.size())
    {
        _isFinished = true;
        return;
    }

    SetupStep();
}

void BattleAction::SetupStep()
{
    if (_curChar.statCom->Is_Player() == true)
        GetCurStep().targets = CLIENT->Get_BattleController()->Get_AliveEnemies();
    else
        GetCurStep().targets = CLIENT->Get_BattleController()->Get_AliveHeroes();
}
