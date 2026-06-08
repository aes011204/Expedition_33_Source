#include "CameraDirector.h"
#include "GameInstance.h"
#include "TargetCamera.h"

CameraDirector::CameraDirector()
{
}

CameraDirector::~CameraDirector()
{
}

void CameraDirector::Request_GamePlay()
{
    if(GAME->Is_ActiveCameraType(ObjectType::TargetCamera))
        return;

    auto camera = GAME->Get_ActiveCamera();

    if(camera == nullptr)
    {
        GAME->Activate_CameraByType(ObjectType::TargetCamera);
        return;
    }

    if(GAME->Activate_CameraByType(ObjectType::TargetCamera))
    {
        _previousCamera = camera;
    }
}

void CameraDirector::Request_Preview()
{
    if(GAME->Is_ActiveCameraType(ObjectType::PreviewCamera))
        return;

    auto camera = GAME->Get_ActiveCamera();

    if(camera == nullptr)
        return;

    if(GAME->Activate_CameraByType(ObjectType::PreviewCamera))
    {
        _previousCamera = camera;
    }
}

void CameraDirector::Request_BattleTurn()
{
    if(GAME->Is_ActiveCameraType(ObjectType::BattleTurnCamera))
        return;

    auto camera = GAME->Get_ActiveCamera();

    if(camera == nullptr)
        return;

    if(GAME->Activate_CameraByType(ObjectType::BattleTurnCamera))
    {
        _previousCamera = camera;
    }
}

void CameraDirector::Request_Cinematic()
{
   // GAME->Activate_CameraByType(ObjectType::CinematicCamera);
}

void CameraDirector::Restore_PreviousCamera()
{
    //이전 상태가 뭐냐에 따라서    GAME->Set_PlayerInputBlocked(false); 이거 할지말지 결정해야하나

    if(_previousCamera == nullptr)
        return;

    if (const auto targetCamera = dynamic_pointer_cast<TargetCamera>(_previousCamera))
        targetCamera->Change_Mode(TargetCameraMode::NORMAL);

    GAME->Change_ActiveCamera(_previousCamera);

    _previousCamera.reset();
}

void CameraDirector::Request_ChangeTargetCameraMode(TargetCameraMode cameraMode)
{
    const Shared<Camera> activeCamera = GAME->Get_ActiveCamera();

    if(!GAME->Is_ActiveCameraType(ObjectType::TargetCamera))
        GAME->Activate_CameraByType(ObjectType::TargetCamera);

   auto targetCamera =  GAME->Get_CameraByType(ObjectType::TargetCamera);

    if(targetCamera == nullptr)
        return;

    dynamic_pointer_cast<TargetCamera>(targetCamera)->Change_Mode(cameraMode);
}

Unique<CameraDirector> CameraDirector::Create()
{
    return make_unique<CameraDirector>();
}
