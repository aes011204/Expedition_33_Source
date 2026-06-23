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
    ANS_SL_MoveTo() = default;
    ~ANS_SL_MoveTo() override = default;

public:
    string Get_TypeName() const override { return "ANS_SL_MoveTo"; }
    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    Shared<GameObject> Resolve_MoveTarget(GameObject* notifyOwner) const;
    void Apply_Position(const AnimNotifyContext& context, const Shared<GameObject>& target, const Vec3& desiredWorldPos) const;

private:
    float _forwardOffset = 3.f;
    Vec3 _startPos = Vec3::Zero;
    Vec3 _goalPos = Vec3::Zero;
};

NS_END
