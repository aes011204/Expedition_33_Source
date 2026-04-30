#include "MainApp.h"

#include "GameInstance.h"
#include "ClientInstance.h"

#include "Level_Loading.h"
#include "ResourceLoader.h"

MainApp::MainApp()
	: _gameInstance { GameInstance::GetInstance() }
{
	// D3D11_SAMPLER_DESC

	//D3D11_BLEND_DESC
	//D3D11_DEPTH_STENCIL_DESC
	//D3D11_RASTERIZER_DESC
	//_context->RSSetState();
	//_context->OMSetBlendState();
	//_context->OMSetDepthStencilState();

}

HRESULT MainApp::Initialize()
{
	/* 엔진을 이용하기위해 엔진프로젝트를 준비시킨다. */
	ENGINE_DESC		EngineDesc{};
	EngineDesc.hInstance = g_hInst;
	EngineDesc.hWnd = g_hWnd;
	EngineDesc.eWinMode = WinMode::Win;
	EngineDesc.iNumLevels = ETOI(LevelType::END);
	EngineDesc.iViewportWidth = g_iWinSizeX;
	EngineDesc.iViewportHeight = g_iWinSizeY;

	if (FAILED(GAME->Initialize_Engine(EngineDesc, _device, _context)))
		return E_FAIL;

	if (FAILED(GAME->Initialize_AssetManager(TEXT("../Bin/Resources"))))
		return E_FAIL;

    if (FAILED(CLIENT->Initialize_Client(_device, _context)))
        return E_FAIL;

	if (FAILED(Ready_Fonts()))
		return E_FAIL;

	if (FAILED(Ready_Prototype_For_Static_Level()))
		return E_FAIL;

	if(FAILED(Ready_StartLevel(LevelType::Logo)))
		return E_FAIL;

	return S_OK;
}

void MainApp::Update(float fTimeDelta)
{
	_gameInstance->Update_Engine(fTimeDelta);

}

HRESULT MainApp::Render()
{
	Vec4			clearColor = Vec4(0.f, 0.f, 1.f, 1.f);
	if (FAILED(_gameInstance->Clear_Buffers(clearColor)))
		return E_FAIL;

	if (FAILED(_gameInstance->Draw()))
		return E_FAIL;

	_gameInstance->Draw_Font(TEXT("Font_Default"), TEXT("니네들은 싸우지마, 욕하지마!"), Vec2(100.f, 0.f));

	if (FAILED(_gameInstance->Present()))
		return E_FAIL;

	return S_OK;
}

HRESULT MainApp::Ready_Fonts()
{
	/*MakeSpriteFont "넥슨lv1고딕 Bold" /FontSize:16 /FastPack /CharacterRegion:0x0020-0x00FF /CharacterRegion:0x3131-0x3163 /CharacterRegion:0xAC00-0xD800 /DefaultCharacter:0xAC00 158ex.spritefont */

	if (FAILED(_gameInstance->Add_Font(TEXT("Font_Default"), TEXT("../Bin/Resources/Fonts/158ex.spritefont"))))
		return E_FAIL;

	return S_OK;
}

HRESULT MainApp::Ready_Prototype_For_Static_Level()
{
	return CLIENT->Ready_StaticResources();
}

HRESULT MainApp::Ready_StartLevel(LevelType eStartLevelID)
{
	if (LevelType::Loading == eStartLevelID)
		return E_FAIL;

	if (FAILED(_gameInstance->Change_Level(ETOI(LevelType::Loading), Level_Loading::Create(_device.Get(), _context.Get(), eStartLevelID))))
		return E_FAIL;

	return S_OK;
}

Shared<MainApp> MainApp::Create()
{
	auto pInstance = make_shared<MainApp>();

	if (FAILED(pInstance->Initialize()))
	{
		MSG_BOX("Failed to Created : MainApp");
		return nullptr;
	}

	return pInstance;
}

void MainApp::Free()
{
	__super::Free();

	CLIENT->Release_Client();
	ClientInstance::DestroyInstance();

	_gameInstance->Release_Engine();
}
