#include "BattleAction.h"

#include "BattleController.h"
#include "ClientInstance.h"
#include "CombatStatCom.h"
NS_BEGIN(Client)

void BattleAction::Set_BattleActionFromRequest(const ActionRequest& request)
{
    ResetAction();

    type = request.type;
    _curChar = request.actor;
    _InitTarget = request.InitTarget;

    if (!request.skillKey.empty())
    {
        if (const Skill_Data* skillData = CLIENT->Get_Skill(request.skillKey))
            _selected_SkillKey = *skillData;
        else
            LOG_WARN("BuildActionFromRequest: skill data not found. skillKey={}", request.skillKey.c_str());
    }

    if (request.itemType != BattleItemType::Invalid)
    {
        if (const Item_Data* itemData = CLIENT->Get_Item(request.itemType))
            _selected_ItemKey = *itemData;
        else
            LOG_WARN("BuildActionFromRequest: item data not found. itemType={}", static_cast<int>(request.itemType));
    }

    _curAnimIndex = 0;
    _curStepIndex = 0;
    _originPos = {};
    _phaseStartPos = {};
    _phaseGoalPos = {};
    _phaseReachedGoal = false;
    _phasePrevRootRatio = 0.f;
    _isFinished = false;



    _steps.clear();
    auto stepSize = _selected_SkillKey.targetingType.size();
    _steps.resize(stepSize);
    for(int i = 0; i < stepSize; i++)
    {
        _steps[i].targetType = _selected_SkillKey.targetingType[i];
    }

    if (!_steps.empty())
        SetupStep();
}

void BattleAction::ResetAction()
{
    type = EBattleContext::Invalid;

    _curChar = {};
    _InitTarget = {};

    _steps.clear();

    _selected_ItemKey = {};
    _selected_SkillKey = {};

    _curAnimIndex = 0;
    _curStepIndex = 0;

    _originPos = {};
    _phaseStartPos = {};
    _phaseGoalPos = {};
    _phaseReachedGoal = false;
    _phasePrevRootRatio = 0.f;

    _isFinished = false;

}
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
    if (_curStepIndex >= _steps.size())
        return;

    auto& step = GetCurStep();
    step.targets.clear();

    vector <BattleChar> charter;

    if (_curChar.statCom->Is_Player() == true)
        charter = CLIENT->Get_BattleController()->Get_AliveEnemies();
    else
        charter = CLIENT->Get_BattleController()->Get_AliveHeroes();

    if (step.targetType == TargetType::Primary)
    {
        step.targets.push_back(_InitTarget);
    }
    else if (step.targetType == TargetType::RandomEnemy)
    {
        if(charter.empty())
        {
            return;
        }

        const int num = Math::RandNum(0, static_cast<int>(charter.size() - 1));

        step.targets.push_back(charter[num]);
    }
    else if (step.targetType == TargetType::AllEnemies)
    {

        step.targets = charter;
    }
}

void BattleAction::Spend_AttackStep()
{
    ++_curStepIndex;

    if (_curStepIndex >= _steps.size())
    {
        //_isFinished = true;
        return;
    }

    SetupStep();
}
void BattleAction::InitActionDirection()
{
    // 연출 시작 전에 필요한 런타임 값 준비

    if (_curChar.actor == nullptr)
        return;

    _originPos = _curChar.actor->Get_Transform()->Get_Position();

    _curAnimIndex = 0;

}

void BattleAction::BeginPhase(const Vec3& startPos, const Vec3& goalPos)
{

        _phaseStartPos = startPos;
        _phaseGoalPos = goalPos;
        _phaseReachedGoal = false;
        _phasePrevRootRatio = 0.f;
    
}

NS_END
