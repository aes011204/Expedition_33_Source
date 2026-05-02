#include "Level_Loading.h"
#include "Loader.h"

#include "GameInstance.h"

#include "Level_Logo.h"
#include "Level_GamePlay.h"


Level_Loading::Level_Loading(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: Level { device, context }
{
}

HRESULT Level_Loading::Initialize(LevelType eNextLevelID)
{
	_nextLevel = eNextLevelID;

	if (FAILED(Ready_Layer_BackGround(TEXT("Layer_BackGround"))))
		return E_FAIL;

	if (FAILED(Ready_Layer_UI(TEXT("Layer_UI"))))
		return E_FAIL;

	_loader = Loader::Create(_device.Get(), _context.Get(), eNextLevelID);

	if (nullptr == _loader)
		return E_FAIL;

	return S_OK;
}

void Level_Loading::Update(float fTimeDelta)
{
	if (true == _loader->isFinished())
	{
		Shared<Level> pNextLevel = nullptr;

		switch (_nextLevel)
		{
		case LevelType::Logo: 
			pNextLevel = Level_Logo::Create(_device.Get(), _context.Get());
			break;
		case LevelType::GamePlay:
			pNextLevel = Level_GamePlay::Create(_device.Get(), _context.Get());
			break;
		}

		if (nullptr == pNextLevel)
		{
			MSG_BOX("Failed to Created : NextLevel");
			return;
		}
			

		if (FAILED(GAME->Change_Level(ETOI(_nextLevel), pNextLevel)))
		{
			MSG_BOX("Failed to Changed : NextLevel");
			return;
		}	
		return;
	}	
}

HRESULT Level_Loading::Render()
{
#ifdef _DEBUG
	_loader->Print_LoadingText();
#endif

	return S_OK;
}

HRESULT Level_Loading::Ready_Layer_BackGround(const wchar_t* pLayerTag)
{
	return S_OK;
}

HRESULT Level_Loading::Ready_Layer_UI(const wchar_t* pLayerTag)
{
	return S_OK;
}

Shared<Level_Loading> Level_Loading::Create(const ComPtr<Device>& device, const ComPtr<Context>& context, LevelType eNextLevelID)
{
	auto pInstance = make_shared<Level_Loading>(device, context);

	if (FAILED(pInstance->Initialize(eNextLevelID)))
	{
		MSG_BOX("Failed to Created : Level_Loading");
		return nullptr;
	}
	return pInstance;
}


void Level_Loading::Free()
{
	__super::Free();

	Safe_Release(_loader);
}




