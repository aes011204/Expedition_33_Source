#pragma once

#include "AnimNotifyState.h"

NS_BEGIN(Engine)
class MovementCom;
class TransformCom;
NS_END

NS_BEGIN(Client)

class ANS_SL_DodgeMove final : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_SL_DodgeMove)

public:
    ANS_SL_DodgeMove() = default;
    ~ANS_SL_DodgeMove() override = default;

public:
    string Get_TypeName() const override { return "ANS_SL_DodgeMove"; }
    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    Shared<GameObject> Resolve_MoveTarget(GameObject* notifyOwner) const;
    Vec3 Resolve_WorldDirection(const Shared<TransformCom>& transform) const;
    void Apply_Position(const AnimNotifyContext& context, const Shared<GameObject>& target, const Vec3& desiredWorldPos) const;

private:
    float _distance = 2.5f;
    Vec2  _moveAxis = Vec2(0.f, 1.f);

    Vec3  _startPos = Vec3::Zero;
    Vec3  _goalPos = Vec3::Zero;
};

NS_END
