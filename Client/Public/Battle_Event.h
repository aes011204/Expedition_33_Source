#pragma once
#include "Engine_Define.h"
#include "CameraPresetTypes.h"

NS_BEGIN(Client)
struct EvtBattlePresentation
{
    BattleCameraMode   cameraMode;
  Shared<Engine::Character> target;

    //ui에 필요한 변수들 추가
};
NS_END
