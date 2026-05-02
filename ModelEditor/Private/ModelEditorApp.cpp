#include "pch.h"
#include "ModelEditorApp.h"
#include "ImGui_Manager.h"
#include "GameInstance.h"

#include <filesystem>

extern HINSTANCE g_hInst;
extern HWND g_hWnd;

NS_BEGIN(ModelEditor)

HRESULT ModelEditorApp::Initialize()
{
	const std::filesystem::path assetRootPath(Get_ClientResourceRoot());
	
	ENGINE_DESC engineDesc{};
	engineDesc.hInstance = g_hInst;
	engineDesc.hWnd = g_hWnd;
	engineDesc.winMode = WinMode::Win;
	engineDesc.numLevels = 1; 
	RECT rc;
	GetClientRect(g_hWnd, &rc);
	engineDesc.viewportWidth = rc.right - rc.left;
	engineDesc.viewportHeight = rc.bottom - rc.top;
	engineDesc.projectName = "ModelEditor";

	if (FAILED(GAME->Initialize_Engine(engineDesc, _device, _context)))
		return E_FAIL;

	if (FAILED(GAME->Initialize_AssetManager(assetRootPath.wstring())))
		return E_FAIL;

	_imguiManager = ImGui_Manager::Create(g_hWnd, _device, _context);
	if (nullptr == _imguiManager)
		return E_FAIL;

	return S_OK;
}

void ModelEditorApp::Priority_Update(float timeDelta)
{
}

void ModelEditorApp::Update(float timeDelta)
{
	GAME->Update_Engine(timeDelta);
	_imguiManager->Update(timeDelta);
}

void ModelEditorApp::Late_Update(float timeDelta)
{
}

HRESULT ModelEditorApp::Render()
{
	const Vec4 clearColor = Vec4(0.1f, 0.1f, 0.1f, 1.f);
	if (FAILED(GAME->Clear_Buffers(clearColor)))
		return E_FAIL;

	if (FAILED(GAME->Draw()))
		return E_FAIL;

	_imguiManager->Render();

	if (FAILED(GAME->Present()))
		return E_FAIL;

	return S_OK;
}

HRESULT ModelEditorApp::Ready_Fonts()
{
	return S_OK;
}

wstring ModelEditorApp::Get_ClientResourceRoot() const
{
	wchar_t modulePath[MAX_PATH] = {};
	if (0 == GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
		return {};

	const std::filesystem::path runtimeRoot = std::filesystem::path(modulePath).parent_path();
	return std::filesystem::absolute(runtimeRoot / L"../../../Client/Bin/Resources").lexically_normal().wstring();
}

wstring ModelEditorApp::Get_ClientResourcePath(const wchar_t* relativePath) const
{
	if (nullptr == relativePath || L'\0' == *relativePath)
		return Get_ClientResourceRoot();

	return (std::filesystem::path(Get_ClientResourceRoot()) / relativePath).lexically_normal().wstring();
}

ModelEditorApp::~ModelEditorApp()
{
	Free();
}

Unique<ModelEditorApp> ModelEditorApp::Create()
{
	auto instance = Unique<ModelEditorApp>(new ModelEditorApp());

	if (FAILED(instance->Initialize()))
	{
		return nullptr;
	}

	return instance;
}

void ModelEditorApp::Free()
{
	_imguiManager.reset();
	if (nullptr != GAME)
		GAME->Release_Engine();
}

NS_END
