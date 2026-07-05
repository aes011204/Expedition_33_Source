#pragma once
#include "Engine_Define.h"
#include "CameraPresetTypes.h"
#include "Character.h"

NS_BEGIN(Client)
struct EvtBattlePresentation
{
    BattleCameraMode   cameraMode = BattleCameraMode::WIDE;
    Shared<Character>  subject = nullptr;
    Shared<Character>  opponent = nullptr;
    string             skillId;
    string             presetId;
    string             clipId;
    bool               useClip = false;
    bool               restartClip = false;
    bool               keepCurrentClip = false;
    bool               skipIfParryActive = false;
    //ui에 필요한 변수들 추가
};

struct EvtBattleCameraShake
{
    float amplitude = 0.f;
    float duration = 0.f;
    float frequencyX = 32.f;
    float frequencyY = 47.f;
    float decayRateX = 0.f;
    float decayRateY = 8.f;
    float verticalRatio = 0.8f;
    float strengthScale = 0.05f;
};
NS_END
