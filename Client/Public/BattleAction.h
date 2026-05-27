#pragma once
#include "Base.h"
#include "Battle_Enum.h"

class BattleAction :
    public Base
{
public:
    struct AttackStep
    {
        TargetType targetType;

        // 실제 타겟은 없음 (초기에는)
        vector<BattleChar> targets; // 이 스텝에서 맞는 대상들 - 만일 광역공격이면 살아있는 애들만 넣기

        // 결과
        vector<bool> dodged;
        vector<bool> parried;
    };

public:
    void Update();
    void NextStep();
    void SetupStep();
    AttackStep& GetCurStep()
    {
        return _steps[_curStepIndex];
    }

    bool Is_FinishAction() { return _isFinished; }
private:
    //-----------DATA------------
    BattleChar _curChar = {};
    vector<AttackStep> _steps;

    Item_Data   _selected_ItemKey = {};
    Skill_Data   _selected_SkillKey = {};

    uint32 _curAnimIndex = {};
    uint32 _curStepIndex = {};

    //------------------------------
    Vec3 _originPos = {};
    Vec3 _phaseStartPos = {};
    Vec3 _phaseGoalPos = {};
    bool _phaseReachedGoal = false;
    float _phasePrevRootRatio = 0.f;

    bool _isFinished = {};
public:

};


