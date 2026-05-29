#pragma once
#include "Base.h"
#include "Battle_Enum.h"
#include "Character_BattleBase.h"
NS_BEGIN(Client)

class BattleAction :
    public Base
{
public:
    struct AttackStep
    {
        TargetType targetType;

        // 실제 타겟은 없음 (초기에는)
        vector<BattleChar> targets; // 이 스텝에서 맞는 대상들 - 만일 광역공격이면 살아있는 애들만 넣기

        //// 결과
        //vector<bool> dodged;
        //vector<bool> parried;
    };

public:
    void Set_BattleActionFromRequest(const ActionRequest& request);
    void ResetAction();
    void Update();
    void NextStep();
    void Spend_AttackStep();

    void InitActionDirection();
    void BeginPhase(const Vec3& startPos, const Vec3& goalPos);
    void ResetCurAnimIndex() { _curAnimIndex = 0; }void AdvanceCurAnimIndex() { ++_curAnimIndex; }
    void SetPhaseReachedGoal(bool reached) { _phaseReachedGoal = reached; }
    void SetPhasePrevRootRatio(float ratio) { _phasePrevRootRatio = ratio; }
    void ResetPhasePrevRootRatio() { _phasePrevRootRatio = 0.f; }
    void SnapActorToPhaseGoal()
    {
        if (_curChar.actor == nullptr)
            return;
        _curChar.actor->Get_Transform()->Set_Position(_phaseGoalPos);
    }
public:
    // getter
    const EBattleContext GetType() const { return type; }
    const BattleChar& Get_CurChar() const { return _curChar; }
    const BattleChar& GetInitTarget() const { return _InitTarget; }
    const vector<AttackStep>& GetSteps() const { return _steps; }
    const Item_Data& GetItemData() const { return _selected_ItemKey; }
    const Skill_Data& GetSkillData() const { return _selected_SkillKey; }
    uint32 GetCurAnimIndex() const { return _curAnimIndex; }
    uint32 GetCurStepIndex() const { return _curStepIndex; }
    const Vec3& GetOriginPos() const { return _originPos; }
    const Vec3& GetPhaseStartPos() const { return _phaseStartPos; }
    const Vec3& GetPhaseGoalPos() const { return _phaseGoalPos; }
    bool HasReachedPhaseGoal() const { return _phaseReachedGoal; }
    float GetPhasePrevRootRatio() const { return _phasePrevRootRatio; }
    AttackStep& GetCurStep() { return _steps[_curStepIndex]; }
    const AttackStep& GetCurStep() const { return _steps[_curStepIndex]; }
    vector<BattleChar>& GetCurStepTarget() { return _steps[_curStepIndex].targets; }
    const vector<BattleChar>& GetCurStepTarget() const { return _steps[_curStepIndex].targets; }

    bool Is_FinishAction() const { return _isFinished; }

    //setter

    void Set_PhaseStartPos(const Vec3& pos){_phaseStartPos = pos;}
    void Set_PhaseGoalPos(const Vec3& pos){_phaseGoalPos = pos;}

    bool Has_ReachedGoal() const {return _phaseReachedGoal;}
private:

    void Set_OriginPosToCurPos() { _originPos = _curChar.actor->Get_Transform()->Get_Position(); };
    void SetupStep();

private:
    //-----------DATA------------
    EBattleContext type = EBattleContext::Invalid;

    BattleChar _curChar = {};
    BattleChar _InitTarget = {};

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
};

NS_END
