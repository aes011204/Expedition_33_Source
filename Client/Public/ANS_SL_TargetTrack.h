#pragma once

#include "AnimNotifyState.h"

NS_BEGIN(Engine)
class MovementCom;
class TransformCom;
NS_END

NS_BEGIN(Client)

class ANS_SL_TargetTrack final : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_SL_TargetTrack)

public:
    ANS_SL_TargetTrack() = default;
    ~ANS_SL_TargetTrack() override = default;

public:
    string Get_TypeName() const override { return "ANS_SL_TargetTrack"; }
    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    Shared<GameObject> Resolve_MoveTarget(GameObject* notifyOwner) const;
    Shared<GameObject> Resolve_LockedTarget() const;
    Vec3 Resolve_TargetGoalPosition(const Shared<GameObject>& moveTarget, const Shared<GameObject>& lockedTarget) const;
    Vec3 Resolve_LookAtRotation(const Vec3& fromWorldPos, const Vec3& toWorldPos, const Vec3& fallbackRotation) const;
    void Apply_Position(const AnimNotifyContext& context, const Shared<GameObject>& target, const Vec3& desiredWorldPos) const;
    void Apply_Rotation(const Shared<GameObject>& target, const Vec3& desiredWorldRot) const;

private:
    bool  _cacheTargetAtBegin = true;
    bool  _applyPosition = true;
    bool  _applyRotation = true;
    bool  _stopMovementOnEnd = true;
    float _targetDistance = 1.25f;

    Vec3 _startPos = Vec3::Zero;
    Vec3 _goalPos = Vec3::Zero;
    Vec3 _startRot = Vec3::Zero;
    Vec3 _goalRot = Vec3::Zero;

    Weak<GameObject> _cachedLockedTarget{};
};

NS_END
