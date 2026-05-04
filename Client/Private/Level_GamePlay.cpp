#include "Level_GamePlay.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "FreeCamera.h"
#include "TargetCamera.h"
#include "PointParticlePreview.h"

Level_GamePlay::Level_GamePlay(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: Level { device, context }
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
    int a = 10;
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

	LightDesc.type          = LightType::Directional;
	LightDesc.direction     = XMFLOAT4(1.f, -1.f, 1.f, 0.f);
	LightDesc.diffuse       = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
	LightDesc.ambient       = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
	LightDesc.specular      = XMFLOAT4(1.f, 1.f, 1.f, 1.f);

	if (FAILED(GAME->Add_Light(LightDesc)))
		return E_FAIL;

    ShadowLightDesc     ShadowDesc{};

    ShadowDesc.eye          = Vec3(-150.f, 150.f, -150.f);
    ShadowDesc.at           = Vec3(0.f, 0.f, 0.f);
    ShadowDesc.fovy         = AI_DEG_TO_RAD(120.f);
    ShadowDesc.aspect       = 16.f / 9.f;
    ShadowDesc.zn           = 0.1f;
    ShadowDesc.zf           = 1000.f;

    if (FAILED(GAME->Update_ShadowLight(ShadowDesc)))
        return E_FAIL;

	return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Camera(const wstring& strLayerTag)
{
	FreeCamera::FREECAMERA_DESC		FreeCameraDesc{};

	FreeCameraDesc.speedPerSec = 10.f;
	FreeCameraDesc.degreePerSec = 180.f;
	FreeCameraDesc.eye = XMFLOAT4(0.f, 10.f, -7.f, 1.f);
	FreeCameraDesc.at = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
	FreeCameraDesc.fovy = XMConvertToRadians(60.f) ;
    FreeCameraDesc.pitch = 0.f;
    FreeCameraDesc.yaw = 0.f;
	FreeCameraDesc.nearPlane = 0.1f;
	FreeCameraDesc.farPlane = 500.f;
	FreeCameraDesc.mouseSensor = 0.05f;

	if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_FreeCamera"),
		ETOI(LevelType::GamePlay), strLayerTag, &FreeCameraDesc)))
		return E_FAIL;

    TargetCamera::TARGETCAMERA_DESC		TargetCameraDesc{};

    TargetCameraDesc.speedPerSec = 10.f;
    TargetCameraDesc.degreePerSec = 180.f;
    TargetCameraDesc.eye = XMFLOAT4(0.f, 20.f, -7.f, 1.f);
    TargetCameraDesc.at = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
    TargetCameraDesc.fovy = XMConvertToRadians(60.f) ;
    TargetCameraDesc.pitch = 0.f;
    TargetCameraDesc.yaw = 0.f;
    TargetCameraDesc.nearPlane = 0.1f;
    TargetCameraDesc.farPlane = 500.f;
    TargetCameraDesc.mouseSensor = 0.2f;
    TargetCameraDesc.followSpeed = 5.f;
    TargetCameraDesc.offset = Vec3 {0.5f, 1.f, 0.f};
    TargetCameraDesc.distance = 2.f;
    TargetCameraDesc.sideOffset = 0.5;
    TargetCameraDesc.heightOffset = 1.f;

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
	for (size_t i = 0; i < 10; i++)
	{
		if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Monster"),
			ETOI(LevelType::GamePlay), strLayerTag)))
			return E_FAIL;
	}

	return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Player(const wstring& strLayerTag)
{
	if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("GameObject_Player"),
		ETOI(LevelType::GamePlay), strLayerTag)))
		return E_FAIL;

    auto goPlayer = CLIENT->Find_Player(ETOI(LevelType::GamePlay));
    if (goPlayer)
    {
        auto tr = goPlayer->Get_Transform();
        if (tr)
        {
            Vec3 pos(10.f, 5.f, 10.f);
            tr->Set_Position(pos);
        }
    }

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

