#include "Level_GamePlay.h"
#include "GameInstance.h"


#include "FreeCamera.h"

Level_GamePlay::Level_GamePlay(ComPtr<Device> device, ComPtr<Context> context)
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

	/*if (FAILED(Ready_Layer_Monster(TEXT("Layer_Monster"))))
		return E_FAIL;*/

	

	return S_OK;
}

void Level_GamePlay::Update(float fTimeDelta)
{
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

	LightDesc.eType = LightType::DIRECTIONAL;
	LightDesc.vDirection = XMFLOAT4(1.f, -1.f, 1.f, 0.f);
	LightDesc.vDiffuse = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
	LightDesc.vAmbient = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
	LightDesc.vSpecular = XMFLOAT4(1.f, 1.f, 1.f, 1.f);

	if (FAILED(GAME->Add_Light(LightDesc)))
		return E_FAIL;


	return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Camera(const wstring& strLayerTag)
{
	FreeCamera::FREECAMERA_DESC		CameraDesc{};

	CameraDesc.fSpeedPerSec = 10.f;
	CameraDesc.fDegreePerSec = 180.f;
	CameraDesc.vEye = XMFLOAT4(0.f, 10.f, -7.f, 1.f);
	CameraDesc.vAt = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
	CameraDesc.fFovy = XMConvertToRadians(60.f) ;
	CameraDesc.fNear = 0.1f;
	CameraDesc.fFar = 500.f;
	CameraDesc.fMouseSensor = 0.05f;

	if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("Prototype_GameObject_FreeCamera"),
		ETOI(LevelType::GamePlay), strLayerTag, &CameraDesc)))
		return E_FAIL;


	return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_BackGround(const wstring& strLayerTag)
{
	if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("Prototype_GameObject_Terrain"),
		ETOI(LevelType::GamePlay), strLayerTag)))
		return E_FAIL;

	if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("Prototype_GameObject_Sky"),
		ETOI(LevelType::GamePlay), strLayerTag)))
		return E_FAIL;



	return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Monster(const wstring& strLayerTag)
{
	for (size_t i = 0; i < 10; i++)
	{
		if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("Prototype_GameObject_Monster"),
			ETOI(LevelType::GamePlay), strLayerTag)))
			return E_FAIL;
	}

	return S_OK;
}

HRESULT Level_GamePlay::Ready_Layer_Player(const wstring& strLayerTag)
{
	if (FAILED(GAME->Add_GameObject(ETOI(LevelType::GamePlay), TEXT("Prototype_GameObject_Player"),
		ETOI(LevelType::GamePlay), strLayerTag)))
		return E_FAIL;

	return S_OK;
}

Shared<Level_GamePlay> Level_GamePlay::Create(ComPtr<Device> device, ComPtr<Context> context)
{
	Shared<Level_GamePlay> pInstance = MakeSharedFromRaw(new Level_GamePlay(device, context));

	if (FAILED(pInstance->Initialize()))
	{
		MSG_BOX("Failed to Created : Level_GamePlay");
		Safe_Release(pInstance);
	}
	return pInstance;
}


void Level_GamePlay::Free()
{
	__super::Free();

}

