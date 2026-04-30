#include "Level_Logo.h"

#include "GameInstance.h"
#include "Level_Loading.h"
#include "BackGround.h"

Level_Logo::Level_Logo(ComPtr<Device> device, ComPtr<Context> context)
	: Level { device, context }
{
}

HRESULT Level_Logo::Initialize()	
{
	if (FAILED(Ready_Layer_BackGround(TEXT("Layer_BackGround"))))
		return E_FAIL;

	return S_OK;
}

void Level_Logo::Update(float fTimeDelta)
{
	if (GAME->KeyDown(KEY_TYPE::SPACE))
	{
		if (FAILED(GAME->Change_Level(ETOI(LevelType::Loading), Level_Loading::Create(_device.Get(), _context.Get(), LevelType::GamePlay))))
			return;

	}

}

HRESULT Level_Logo::Render()
{
#ifdef _DEBUG
	SetWindowText(g_hWnd, TEXT("Logo Level"));
#endif

	return S_OK;
}

HRESULT Level_Logo::Ready_Layer_BackGround(const wstring& strLayerTag)
{
	//BackGround::BACKGROUND_DESC		BackGroundDesc{};

	//BackGroundDesc.iFlag = 0;
	//BackGroundDesc.fSpeedPerSec = 1.f;
	//BackGroundDesc.fRotationPerSec = 1.f;
	

	if (FAILED(GAME->Add_GameObject(ETOI(LevelType::Logo), TEXT("Prototype_GameObject_BackGround"),
		ETOI(LevelType::Logo), strLayerTag/*, &BackGroundDesc*/)))
		return E_FAIL;


	return S_OK;
}

Shared<Level_Logo> Level_Logo::Create(ComPtr<Device> device, ComPtr<Context> context)
{
	Shared<Level_Logo> pInstance = MakeSharedFromRaw(new Level_Logo(device, context));

	if (FAILED(pInstance->Initialize()))
	{
		MSG_BOX("Failed to Created : Level_Logo");
		Safe_Release(pInstance);
	}
	return pInstance;
}


void Level_Logo::Free()
{
	__super::Free();

}


