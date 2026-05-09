#include "PersistentObjectManager.h"
#include "GameInstance.h"
#include "FreeCamera.h"
#include "TargetCamera.h"

HRESULT PersistentObjectManager::SpawnStaticCamera()
{
    FreeCamera::FREECAMERA_DESC CameraDesc{};
    CameraDesc.speedPerSec = 10.f;
    CameraDesc.degreePerSec = 180.f;
    CameraDesc.eye = XMFLOAT4(0.f, 10.f, -7.f, 1.f);
    CameraDesc.at = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
    CameraDesc.fovy = XMConvertToRadians(60.f);
    CameraDesc.nearPlane = 0.1f;
    CameraDesc.farPlane = 500.f;
    CameraDesc.mouseSensor = 0.05f;

    if (FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), TEXT("GameObject_FreeCamera"),
        ETOI(LevelType::Static), TEXT("Layer_Camera"), &CameraDesc)))
    {
        return E_FAIL;
    }

    TargetCamera::TARGETCAMERA_DESC TargetCameraDesc{};
    TargetCameraDesc.speedPerSec = 10.f;
    TargetCameraDesc.degreePerSec = 180.f;
    TargetCameraDesc.eye = XMFLOAT4(0.f, 20.f, -7.f, 1.f);
    TargetCameraDesc.at = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
    TargetCameraDesc.fovy = XMConvertToRadians(60.f);
    TargetCameraDesc.nearPlane = 0.1f;
    TargetCameraDesc.farPlane = 500.f;
    TargetCameraDesc.mouseSensor = 0.06f;
    TargetCameraDesc.followSpeed = 4.f;
    TargetCameraDesc.distance = 1.8f;
    TargetCameraDesc.sideOffset = 0.f;
    TargetCameraDesc.heightOffset = 0.5f;
    TargetCameraDesc.aimOffset = 0.5f;
    TargetCameraDesc.pitchLookScale = 0.5f;
    TargetCameraDesc.pitchMin = -70.f;
    TargetCameraDesc.pitchMax = 80.f;
    TargetCameraDesc.lookHeight = 0.8f;
    TargetCameraDesc.pivotHeight = 0.3f;
    TargetCameraDesc.mouseSensitivity = 0.5f;

    if (FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), TEXT("GameObject_TargetCamera"),
        ETOI(LevelType::Static), TEXT("Layer_Camera"), &TargetCameraDesc)))
    {
        return E_FAIL;
    }

    return S_OK;
}

HRESULT PersistentObjectManager::SpawnStaticPlayer()
{
    if (FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), TEXT("GameObject_Player"),
        ETOI(LevelType::Static), TEXT("Layer_Player"))))
    {
        return E_FAIL;
    }

    return S_OK;
}

HRESULT PersistentObjectManager::SpawnStaticSky()
{
    if (FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), TEXT("GameObject_Sky"),
        ETOI(LevelType::Static), TEXT("Layer_BackGround"))))
    {
        return E_FAIL;
    }

    return S_OK;
}

HRESULT PersistentObjectManager::Spawn_StaticObjects()
{
    CHECK_FAILED(SpawnStaticCamera(), E_FAIL);
    CHECK_FAILED(SpawnStaticPlayer(), E_FAIL);
    CHECK_FAILED(SpawnStaticSky(), E_FAIL);

    return S_OK;
}

Shared<GameObject> PersistentObjectManager::Find_Player() const
{
    return GAME->Find_FirstObject(ETOI(LevelType::Static), TEXT("Layer_Player"));
}

Unique<PersistentObjectManager> PersistentObjectManager::Create()
{
    return make_unique<PersistentObjectManager>();
}

void PersistentObjectManager::Free()
{
    __super::Free();
}
