#include "Level_GamePlay.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "FreeCamera.h"
#include "TargetCamera.h"
#include "PointParticlePreview.h"

Level_GamePlay::Level_GamePlay(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Level{ device, context }
{
}

HRESULT Level_GamePlay::Initialize()
{
    if (FAILED(Ready_Lights()))
        return E_FAIL;

    if (FAILED(Ready_Layer_Camera(TEXT("Layer_Camera"))))
        return E_FAIL;

    if (FAILED(Ready_Layer_BackGround(TEXT("Layer_BackGround"))))
        return E_FAIL;

    if (FAILED(Ready_Layer_Player(TEXT("Layer_Player"))))
        return E_FAIL;

    if (FAILED(Ready_Layer_Monster(TEXT("Layer_Monster"))))
        return E_FAIL;

    return S_OK;
}

void Level_GamePlay::BeginPlay()
{
    Level::BeginPlay();

    
}

void Level_GamePlay::Update(float fTimeDelta)
{
    // 테스트
#ifdef _DEBUG
    static float accumulatedTime = 0.f;
    accumulatedTime += fTimeDelta;

    const int fakeHP = 100 - static_cast<int>(accumulatedTime) % 100;
    const wchar_t* stateText = (static_cast<int>(accumulatedTime) % 2 == 0) ? L"Idle" : L"Run";

    GAME->Print(L"프린트 테스트", duration(0.f));
    GAME->Print(L"Player State : {}", stateText, duration(0.f));
    GAME->Print(L"Time : {:.2f}", accumulatedTime, duration(0.f), color(Vec4(1.f, 1.f, 0.f, 1.f)));
#endif
}

HRESULT Level_GamePlay::Render()
{
#ifdef _DEBUG
    SetWindowText(g_hWnd, TEXT("Level GamePlay"));


#endif
    return S_OK;
}

HRESULT Level_GamePlay::Ready_Lights()
{
    LIGHT_DESC			LightDesc{};

    LightDesc.type = LightType::Directional;
    LightDesc.direction = XMFLOAT4(1.f, -1.f, 1.f, 0.f);
    LightDesc.diffuse = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
    LightDesc.ambient = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
    LightDesc.specular = XMFLOAT4(1.f, 1.f, 1.f, 1.f);

    if (FAILED(GAME->Add_Light(LightDesc)))
        return E_FAIL;


    return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Camera(const wstring& strLayerTag)
{
	FreeCamera::FREECAMERA_DESC		CameraDesc{};

	CameraDesc.speedPerSec = 10.f;
	CameraDesc.degreePerSec = 180.f;
	CameraDesc.eye = XMFLOAT4(0.f, 10.f, -7.f, 1.f);
	CameraDesc.at = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
	CameraDesc.fovy = XMConvertToRadians(60.f) ;
	CameraDesc.nearPlane = 0.1f;
	CameraDesc.farPlane = 500.f;
	CameraDesc.mouseSensor = 0.05f;

	if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_FreeCamera"),
		ETOI(LevelType::GamePlay), strLayerTag, &CameraDesc)))
		return E_FAIL;

    TargetCamera::TARGETCAMERA_DESC		TargetCameraDesc{};

    TargetCameraDesc.speedPerSec = 10.f;
    TargetCameraDesc.degreePerSec = 180.f;
    TargetCameraDesc.eye = XMFLOAT4(0.f, 20.f, -7.f, 1.f);
    TargetCameraDesc.at = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
    TargetCameraDesc.fovy = XMConvertToRadians(60.f);
    TargetCameraDesc.pitch = 0.f;
    TargetCameraDesc.yaw = 0.f;
    TargetCameraDesc.nearPlane = 0.1f;
    TargetCameraDesc.farPlane = 500.f;
    TargetCameraDesc.mouseSensor = 0.06f;
    TargetCameraDesc.followSpeed = 8.f;
    TargetCameraDesc.offset = Vec3 {0.5f, 1.f, 0.f};
    TargetCameraDesc.distance = 1.8f;
    TargetCameraDesc.sideOffset = 0.f;
    TargetCameraDesc.heightOffset = 0.5f;
    TargetCameraDesc.aimOffset = 0.5f;
    TargetCameraDesc.pitchLookScale = 0.5;
    TargetCameraDesc.pitchMin = -70.f;
    TargetCameraDesc.pitchMax = 80.f;
    TargetCameraDesc.lookHeight = 0.8f;
    TargetCameraDesc.pivotHeight =0.3f;

    if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_TargetCamera"),
        ETOI(LevelType::GamePlay), strLayerTag, &TargetCameraDesc)))
        return E_FAIL;

	return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_BackGround(const wstring& strLayerTag)
{
    if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Terrain"),
        ETOI(LevelType::GamePlay), strLayerTag)))
        return E_FAIL;

    if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Sky"),
        ETOI(LevelType::GamePlay), strLayerTag)))
        return E_FAIL;

    if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_PointParticlePreview"),
        ETOI(LevelType::GamePlay), strLayerTag)))
        return E_FAIL;



	return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Monster(const wstring& strLayerTag)
{

		if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Monster"),
			ETOI(LevelType::GamePlay), strLayerTag)))
			return E_FAIL;
	

    return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Player(const wstring& strLayerTag)
{
    if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Player"),
        ETOI(LevelType::GamePlay), strLayerTag)))
        return E_FAIL;



    return S_OK;
}

Shared<Level_GamePlay> Level_GamePlay::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto pInstance = make_shared<Level_GamePlay>(device, context);

    if (FAILED(pInstance->Initialize()))
    {
        MSG_BOX("Failed to Created : Level_GamePlay");
        return nullptr;
    }
    return pInstance;
}


void Level_GamePlay::Free()
{
    __super::Free();

}

