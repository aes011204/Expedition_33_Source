#include "pch.h"
#include "MainApp.h"

#include "GameInstance.h"
#include "ClientInstance.h"
#include "Level_Loading.h"
#include "ResourceLoader.h"

using namespace Engine;

NS_BEGIN(GameLauncherApp)

MainApp::MainApp()
    : _gameInstance{ GameInstance::GetInstance() }
{
}

HRESULT MainApp::Initialize()
{
    const fs::path assetRootPath(Get_ClientResourceRoot());
    if (!fs::exists(assetRootPath) || !fs::is_directory(assetRootPath))
        return E_FAIL;

    ENGINE_DESC engineDesc{};
    engineDesc.hInstance = g_hInst;
    engineDesc.hWnd = g_hWnd;
    engineDesc.winMode = WinMode::Win;
    engineDesc.numLevels = ETOI(LevelType::END);
    engineDesc.viewportWidth = g_iWinSizeX;
    engineDesc.viewportHeight = g_iWinSizeY;
    engineDesc.projectName = "GameLauncher";

    if (FAILED(GAME->Initialize_Engine(engineDesc, _device, _context)))
        return E_FAIL;

    if (FAILED(GAME->Initialize_AssetManager(assetRootPath.wstring())))
        return E_FAIL;

    if (FAILED(CLIENT->Initialize_Client(_device, _context)))
        return E_FAIL;

    if (FAILED(Ready_Fonts()))
        return E_FAIL;

    if (FAILED(Ready_Prototype_For_Static_Level()))
        return E_FAIL;

    if (FAILED(Ready_StartLevel(LevelType::Logo)))
        return E_FAIL;

    return S_OK;
}

void MainApp::Update(float fTimeDelta)
{
    _gameInstance->Update_Engine(fTimeDelta);
}

HRESULT MainApp::Render()
{
    const Vec4 clearColor = Vec4(0.f, 0.f, 1.f, 1.f);
    if (FAILED(_gameInstance->Clear_Buffers(clearColor)))
        return E_FAIL;

    if (FAILED(_gameInstance->Draw()))
        return E_FAIL;

    _gameInstance->Draw_Font(TEXT("Font_Default"), TEXT("니네들은 싸우지말라고하시네요"), Vec2(100.f, 0.f));

    if (FAILED(_gameInstance->Present()))
        return E_FAIL;

    return S_OK;
}

HRESULT MainApp::Ready_Fonts()
{
    const wstring fontPath = Get_ClientResourcePath(L"Fonts/158ex.spritefont");
    if (!fs::exists(fontPath))
        return E_FAIL;

    if (FAILED(_gameInstance->Add_Font(TEXT("Font_Default"), fontPath.c_str())))
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

    if (FAILED(_gameInstance->Change_Level(
        ETOI(LevelType::Loading),
        Level_Loading::Create(_device.Get(), _context.Get(), eStartLevelID))))
    {
        return E_FAIL;
    }

    return S_OK;
}

wstring MainApp::Get_ClientResourceRoot() const
{
    wchar_t modulePath[MAX_PATH] = {};
    if (0 == GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
        return {};

    const fs::path runtimeRoot = fs::path(modulePath).parent_path();
    return fs::absolute(runtimeRoot / L"../../../Client/Bin/Resources").lexically_normal().wstring();
}

wstring MainApp::Get_ClientResourcePath(const wchar_t* relativePath) const
{
    if (nullptr == relativePath || L'\0' == *relativePath)
        return Get_ClientResourceRoot();

    return (fs::path(Get_ClientResourceRoot()) / relativePath).lexically_normal().wstring();
}

Unique<MainApp> MainApp::Create()
{
    Unique<MainApp> pInstance{ new MainApp() };

    if (FAILED(pInstance->Initialize()))
    {
        MSG_BOX("Failed to Created : GameLauncher MainApp");
        pInstance->Free();
        return nullptr;
    }

    return pInstance;
}

void MainApp::Free()
{
    __super::Free();

    CLIENT->Release_Client();
    ClientInstance::DestroyInstance();

    if (nullptr != _gameInstance)
        _gameInstance->Release_Engine();
}

NS_END
