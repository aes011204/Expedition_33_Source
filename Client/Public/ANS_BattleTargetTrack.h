#pragma once

#include "AnimNotifyState.h"

NS_BEGIN(Client)

class ANS_BattleTargetTrack final : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_BattleTargetTrack)

public:
    ANS_BattleTargetTrack() = default;
    ~ANS_BattleTargetTrack() override = default;

public:
    string Get_TypeName() const override { return "ANS_BattleTargetTrack"; }
    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    Shared<GameObject> Resolve_MoveTarget(GameObject* notifyOwner) const;
    Shared<GameObject> Resolve_BattleTarget() const;
    Vec3 Resolve_LookAtRotation(const Vec3& fromWorldPos, const Vec3& toWorldPos, const Vec3& fallbackRotation) const;
    Vec3 Resolve_LerpedRotation(const Vec3& fromRot, const Vec3& toRot, float t) const;
    void Apply_Rotation(const Shared<GameObject>& target, const Vec3& desiredRot) const;

private:
    bool _yawRotateToTarget = true;
    bool _cacheTargetAtBegin = false;
    bool _useWorldRotation = false;

    Vec3 _startRot = Vec3::Zero;
    Vec3 _goalRot = Vec3::Zero;
    Weak<GameObject> _cachedBattleTarget{};
};

NS_END
