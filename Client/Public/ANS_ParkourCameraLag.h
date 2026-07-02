#pragma once

#include "AnimNotifyState.h"

NS_BEGIN(Client)

class Player_WorldBase;

class ANS_ParkourCameraLag final : public AnimNotifyState
{
    GENERATED_ANIM_NOTIFY_STATE(ANS_ParkourCameraLag)

public:
    ANS_ParkourCameraLag() = default;
    ~ANS_ParkourCameraLag() override = default;

public:
    string Get_TypeName() const override { return "ANS_ParkourCameraLag"; }
    void On_Begin(const AnimNotifyContext& context) override;
    void On_Tick(const AnimNotifyContext& context) override;
    void On_End(const AnimNotifyContext& context) override;

private:
    Shared<Player_WorldBase> Resolve_Player(GameObject* notifyOwner) const;
};

NS_END
