#pragma once
#include <TransformCom.h>
#include "Base.h"
#include "BattleAction.h"

NS_BEGIN(Engine)
class ModelCom;
NS_END

NS_BEGIN(Client)
class BattleActionExecutor :
    public Base
{
public:
    void BeginAction(BattleAction& action);
    void Update(float timeDelta, BattleAction& action);
    void OnActionEndNotify(BattleAction& action);
    void Complete_CurPhase(BattleAction& action);

public:
    bool IsFinished() const {return _isFinished;}
    void Set_ReStart() { _isFinished = false; }
    void Reset()
    {
        Reset_ActivePhasePlayRate();
        _isFinished = true;
        _testPrevRatio = 0.f;
    }
private:
    void Begin_ActionPhase(BattleAction& action); //시작시 들어감
    void Update_ActionMovement(float timeDelta, BattleAction& action);

    void Update_BasicMovement(BattleAction& action, const Shared<TransformCom>& transform, const Vec3& currentPos);
    void Update_RootMotionToTarget(BattleAction& action, const Shared<TransformCom>& transform, const Vec3& currentPos, float timeDelta);
    void Update_RatioMoveTo(BattleAction& action, const Shared<TransformCom>& transform, const Vec3& currentPos);

    void FinishCurrentPhaseAtGoal(BattleAction& action);

    Vec3 Resolve_AnchorPosition(const BattleAction& action, BattleTargetAnchor anchor) const;
    Vec3 Resolve_PhaseGoalPosition(const BattleAction& action, const BattleActionPhase& phase) const;
    void Face_ActionTarget(BattleAction& action, const Vec3& goalPos);
    void SmoothFaceActionTarget(BattleAction& action, const Vec3& goalPos, float timeDelta);

    void Apply_PhasePlayRate(const BattleActionPhase& phase, const Shared<Engine::ModelCom>& model);
    void Reset_ActivePhasePlayRate();

private:
    bool _isFinished = {true};
    float _testPrevRatio = {};

    Shared<ModelCom> _activePhaseModel{};

public:
    static Shared<BattleActionExecutor> Create();
    void Free() override;
};

NS_END
