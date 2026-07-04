#pragma once

#include "AnimNotifyState.h"

NS_BEGIN(Engine)
class MovementCom;
class TransformCom;
NS_END

NS_BEGIN(Client)

class ANS_SL_MoveTo final : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_SL_MoveTo)

public:
    enum class MoveMode
    {
        TargetForwardOffset,
        ForwardDistance,
        BackwardDistance,
    };

public:
    ANS_SL_MoveTo() = default;
    ~ANS_SL_MoveTo() override = default;

public:
    string Get_TypeName() const override { return "ANS_SL_MoveTo"; }
    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    Shared<GameObject> Resolve_MoveTarget(GameObject* notifyOwner) const;
    Vec3 Resolve_GoalPosition(const Shared<GameObject>& moveTarget, const Shared<GameObject>& target) const;
    void Apply_Position(const AnimNotifyContext& context, const Shared<GameObject>& target, const Vec3& desiredWorldPos);

private:
    MoveMode _moveMode = MoveMode::TargetForwardOffset;
    float _forwardOffset = 3.f;
    float _distance = 2.f;
    Vec3 _startPos = Vec3::Zero;
    Vec3 _goalPos = Vec3::Zero;
    bool _goalClampedByPlayer = false;
};

NS_END
