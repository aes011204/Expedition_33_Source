#pragma once
#include "Engine_Define.h"
#include "CameraPresetTypes.h"
#include "Character.h"

NS_BEGIN(Client)
struct EvtBattlePresentation
{
  BattleCameraMode   cameraMode;
  Shared<Character>  subject;
  Shared<Character>  opponent;
  string             skillId;
    //ui에 필요한 변수들 추가
};
NS_END
