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
