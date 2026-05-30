#pragma once
#include "AnimNotify.h"
#include "CameraPresetTypes.h"

NS_BEGIN(Client)

class AN_BattleShot : public AnimNotify
{
    GENERATED_ANIM_NOTIFY(AN_BattleShot)

public:
    AN_BattleShot() = default;
    virtual ~AN_BattleShot() = default;

public:
    string Get_TypeName() const override { return "AN_BattleShot";}
    void Execute(const AnimNotifyContext& context) override;

private:
    BattleCameraMode _cameraMode = BattleCameraMode::PLAYERATTACK;
    string           _presetId = {};
    string           _clipId = {};
    bool             _restartClip = false;
    bool             _useClip = false;
};

NS_END
