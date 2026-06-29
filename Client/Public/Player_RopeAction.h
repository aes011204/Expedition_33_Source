#pragma once

#include "Player_State.h"

NS_BEGIN(Client)

class Player_RopeAction final : public Player_State
{
public:
    Player_RopeAction(const Shared<Player_WorldBase>& owner);
    ~Player_RopeAction() override = default;

public:
    HRESULT Initialize() override;
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void LateUpdate(float timeDelta) override;
    void ExitInvoke() override;

public:
    static Shared<Player_RopeAction> Create(const Shared<Player_WorldBase>& owner);
    void Free() override;

private:
    enum class RopePhase
    {
        None,
        Start,
        Travel,
        End,
    };

private:
    void Enter_TravelPhase();
    void Enter_EndPhase();
    void Update_StartPhase(float timeDelta);
    void Update_TravelPhase(float timeDelta);
    void Update_EndPhase(float timeDelta);
    bool Is_UpDownRopeRequest() const;
    Vec3 Resolve_LandingPoint(const Vec3& targetPoint) const;
    Quat Resolve_TargetLookRotation(const Shared<TransformCom>& transform, const Vec3& targetPoint) const;
    void Apply_LookBlend(
        const Shared<TransformCom>& transform,
        const Quat& fromRotation,
        const Quat& toRotation,
        float ratio) const;
    void Finish_RopeAction();

private:
    WorldInteractionSpot::WorldInteractionRequest _request{};
    RopePhase _phase = RopePhase::None;

    Vec3 _travelStartPos = Vec3::Zero;
    Vec3 _travelEndPos = Vec3::Zero;
    Vec3 _startAlignFromPos = Vec3::Zero;
    Vec3 _endMoveStartPos = Vec3::Zero;
    Vec3 _endMoveTargetPos = Vec3::Zero;
    Quat _startLookFromRotation = Quat::Identity;
    Quat _startLookToRotation = Quat::Identity;
    Quat _endLookFromRotation = Quat::Identity;
    Quat _endLookToRotation = Quat::Identity;

    float _travelDistance = 0.f;
    float _totalTravelDistance = 0.f;
    float _prevRootMotionRatio = 0.f;
    float _endLookStartRootMotionRatio = 0.f;
    bool _hasActiveRequest = false;
};

NS_END
