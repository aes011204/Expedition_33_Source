#include "CameraDirector.h"
#include "GameInstance.h"

CameraDirector::CameraDirector()
{
}

CameraDirector::~CameraDirector()
{
}

void CameraDirector::RequestGamePlay()
{
    GAME->Activate_CameraByType(ObjectType::TargetCamera);
}

void CameraDirector::RequestPreview()
{
    GAME->Activate_CameraByType(ObjectType::PreviewCamera);
}

void CameraDirector::RequestBattleTurn()
{
    GAME->Activate_CameraByType(ObjectType::BattleTurnCamera);
}

void CameraDirector::RequestCinematic()
{
  //  GAME->Activate_CameraByType(ObjectType::CinematicCamera);
}
