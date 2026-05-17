#pragma once
#include "Base.h"
#include "Battle_Enum.h"

NS_BEGIN(Client)
struct Action;


class BattleActionExecutor :
    public Base
{
public:
    void BeginAction(Action& action);
    void Update(float timeDelta, Action& action);
    void OnActionEndNotify(Action& action);

public:
    bool IsFinished() const
    {
        return _isFinished;
    }
private:
    void Begin_ActionPhase(Action& action);
    Vec3 Resolve_AnchorPosition(const Action& action, BattleTargetAnchor anchor) const;
    Vec3 Resolve_PhaseGoalPosition(const Action& action, const BattleActionPhase& phase) const;
    void Face_ActionTarget(Action& action, const Vec3& goalPos);
    void Update_ActionMovement(float timeDelta, Action& action);

private:
    bool _isFinished = {false};
    float _testPrevRatio = {};
public:
    static Shared<BattleActionExecutor> Create();
    void Free() override;
};

NS_END
