#pragma once

#include "Player_State.h"

NS_BEGIN(Client)

class Player_GrappleAction final : public Player_State
{
public:
    Player_GrappleAction(const Shared<Player_WorldBase>& owner);
    ~Player_GrappleAction() override = default;

public:
    HRESULT Initialize() override;
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void LateUpdate(float timeDelta) override;
    void ExitInvoke() override;

public:
    static Shared<Player_GrappleAction> Create(const Shared<Player_WorldBase>& owner);
    void Free() override;

private:
    bool Is_GrapplingRequest() const;
    Quat Resolve_TargetLookRotation(const Shared<TransformCom>& transform, const Vec3& targetPoint) const;
    void Apply_LookBlend(
        const Shared<TransformCom>& transform,
        const Quat& fromRotation,
        const Quat& toRotation,
        float ratio) const;
    void Begin_GrappleLaunch(const Shared<TransformCom>& transform);
    void Finish_GrappleAction();

private:
    WorldInteractionSpot::WorldInteractionRequest _request{};

    Vec3 _launchStartPos = Vec3::Zero;
    Vec3 _launchTargetPos = Vec3::Zero;
    Quat _launchLookFromRotation = Quat::Identity;
    Quat _launchLookToRotation = Quat::Identity;
    float _lookPrepareElapsed = 0.f;

    bool _hasActiveRequest = false;
    bool _isPreparingLook = false;
    bool _isLaunching = false;
};

NS_END
