#include "Loader.h"
#include "GameInstance.h"
#include "ResourceLoader.h"

#include "BackGround.h"
#include "Terrain.h"
#include "FreeCamera.h"
#include "Monster.h"
#include "Player.h"
#include "Body_Player.h"
#include "Weapon.h"
#include "Sky.h"

Loader::Loader(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: _device { device }
	, _context { context }
{
}

unsigned int APIENTRY ThreadMain(void* pArg)
{
	Loader* pLoader = static_cast<Loader*>(pArg);
	if (nullptr == pLoader)
		return 1;

	if (FAILED(pLoader->Loading()))
		return 1;
	
	return 0;
}

HRESULT Loader::Initialize(LevelType eNextLevelID)
{
	_nextLevel = eNextLevelID;

	InitializeCriticalSection(&_criticalSection);

	_threadHandle = (HANDLE)_beginthreadex(nullptr, 0, ThreadMain, this, 0, nullptr);

	if (0 == _threadHandle)
	{
		MSG_BOX("Failed To Created : Thread");
		return E_FAIL;
	}

	return S_OK;
}

HRESULT Loader::Loading()
{
	
	HRESULT		hr = {};

	EnterCriticalSection(&_criticalSection);

	CoInitializeEx(nullptr, COINIT_MULTITHREADED);


	switch (_nextLevel)
	{
	case LevelType::Logo:
		hr = Loading_For_LogoLevel();
		break;

	case LevelType::GamePlay:
		hr = Loading_For_GamePlayLevel();
		break;

    case LevelType::MapTest:
        hr = Loading_For_MapTestLevel();
        break;

    case LevelType::Stage01:
        hr = Loading_For_Stage01Level();
        break;

	default:
		hr = E_FAIL;
	}

	CoUninitialize();
	
	LeaveCriticalSection(&_criticalSection);

	return hr;
}

#ifdef _DEBUG

HRESULT Loader::Print_LoadingText()
{
	SetWindowText(g_hWnd, _loadingText);

	return S_OK;
}

#endif

HRESULT Loader::Load_ResourceTablesForLevel(LevelType eLevel)
{
	Shared<ResourceLoader> pResourceLoader = ResourceLoader::Create(_device, _context);
	if (nullptr == pResourceLoader)
		return E_FAIL;

	return pResourceLoader->Load_LevelTables(eLevel);
}

HRESULT Loader::Loading_For_LogoLevel()
{
	lstrcpy(_loadingText, TEXT("json 리소스 테이블을 로딩 중입니다."));
	if (FAILED(Load_ResourceTablesForLevel(LevelType::Logo)))
	{
		MSG_BOX("Faild to Load Resource Tables : Logo");
		return E_FAIL;
	}

	lstrcpy(_loadingText, TEXT("로딩이 완료되었습니다."));

	_isFinished = true;
	
	return S_OK;
}

HRESULT Loader::Loading_For_GamePlayLevel()
{
	lstrcpy(_loadingText, TEXT("json 리소스 테이블을 로딩 중입니다."));

	if (FAILED(Load_ResourceTablesForLevel(LevelType::GamePlay)))
	{
		MSG_BOX("Faild to Load Resource Tables : GamePlay");
		return E_FAIL;
	}

	lstrcpy(_loadingText, TEXT("로딩이 완료되었습니다."));

	_isFinished = true;

	return S_OK;
}

HRESULT Loader::Loading_For_MapTestLevel()
{
    lstrcpy(_loadingText, TEXT("json 리소스 테이블을 로딩 중입니다."));

    if (FAILED(Load_ResourceTablesForLevel(LevelType::MapTest)))
    {
        MSG_BOX("Faild to Load Resource Tables : MapTest");
        return E_FAIL;
    }

    lstrcpy(_loadingText, TEXT("로딩이 완료되었습니다."));
    _isFinished = true;

    return S_OK;
}

HRESULT Loader::Loading_For_Stage01Level()
{
    lstrcpy(_loadingText, TEXT("json 리소스 테이블을 로딩 중입니다."));

    if (FAILED(Load_ResourceTablesForLevel(LevelType::Stage01)))
    {
        MSG_BOX("Faild to Load Resource Tables : Stage01");
        return E_FAIL;
    }

    lstrcpy(_loadingText, TEXT("로딩이 완료되었습니다."));
    _isFinished = true;

    return S_OK;
}

Shared<Loader> Loader::Create(const ComPtr<Device>& device, const ComPtr<Context>& context, LevelType eNextLevelID)
{
	auto pInstance = make_shared<Loader>(device, context);

	if (FAILED(pInstance->Initialize(eNextLevelID)))
	{
		MSG_BOX("Failed to Created : Loader");
		return nullptr;
	}
	return pInstance;
}

void Loader::Free()
{
	__super::Free();

	WaitForSingleObject(_threadHandle, INFINITE);
	CloseHandle(_threadHandle);

	DeleteCriticalSection(&_criticalSection);
}





