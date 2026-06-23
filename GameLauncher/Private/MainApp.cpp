#include "pch.h"
#include "MainApp.h"

#include "GameInstance.h"
#include "ClientInstance.h"
#include "Level_Loading.h"
#include "ResourceLoader.h"
#include "RuntimeCheckpointService.h"
#include <shellapi.h>

using namespace Engine;

NS_BEGIN(GameLauncherApp)

MainApp::MainApp()
{
}

HRESULT MainApp::Initialize(const wstring& commandLine)
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
    engineDesc.swapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    engineDesc.projectName = "GameLauncher";

    if (FAILED(GAME->Initialize_Engine(engineDesc, _device, _context)))
        return E_FAIL;

    if (FAILED(GAME->Initialize_AssetManager(assetRootPath.wstring())))
        return E_FAIL;

    if (FAILED(CLIENT->Initialize_Client(_device, _context)))
        return E_FAIL;

    if (FAILED(Ready_Fonts()))
        return E_FAIL;

    Parse_CommandLine(commandLine);

    LevelType startLevel = LevelType::MainTitle;
    if (!_startupCheckpointPath.empty())
    {
        _hasStartupCheckpoint = Client::RuntimeCheckpointIO::Load_Msgpack(_startupCheckpointPath, _startupCheckpoint);
        if (_hasStartupCheckpoint)
            startLevel = _startupCheckpoint.levelType;
        else
            LOG_WARN("[GameLauncher] checkpoint load failed: {}", String::ToString(_startupCheckpointPath.wstring()));
    }

    if (FAILED(Ready_StartLevel(startLevel)))
        return E_FAIL;

    return S_OK;
}

void MainApp::Update(float fTimeDelta)
{
    GAME->Update_Engine(fTimeDelta, [](float dt)
    {
        CLIENT->Update_Client(dt);
    });

    Try_ApplyStartupCheckpoint();
}

HRESULT MainApp::Render()
{
    GAME->Bind_BackBuffer();

    const bool isLoading = GAME->Current_LevelIndex() == ETOI(LevelType::Loading);
    const Vec4 clearColor = isLoading
        ? Vec4(0.f, 0.f, 0.f, 1.f)
        : Vec4(0.f, 0.f, 1.f, 1.f);

    if (FAILED(GAME->Clear_Buffers(clearColor)))
        return E_FAIL;

    if (FAILED(GAME->Draw()))
        return E_FAIL;

    if (FAILED(GAME->Present()))
        return E_FAIL;

    return S_OK;
}

HRESULT MainApp::Ready_Fonts()
{
    const wstring fontPath = Get_ClientResourcePath(L"Fonts/158ex.spritefont");
    if (!fs::exists(fontPath))
        return E_FAIL;

    if (FAILED(GAME->Add_Font(TEXT("Font_Default"), fontPath.c_str())))
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

    if (FAILED(GAME->Change_Level(
        ETOI(LevelType::Loading),
        Level_Loading::Create(_device.Get(), _context.Get(), eStartLevelID))))
    {
        return E_FAIL;
    }

    return S_OK;
}

void MainApp::Parse_CommandLine(const wstring& commandLine)
{
    _startupCheckpointPath.clear();
    _hasStartupCheckpoint = false;
    _startupCheckpointApplied = false;

    if (commandLine.empty())
        return;

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(commandLine.c_str(), &argc);
    if (argv == nullptr)
        return;

    for (int i = 0; i < argc; ++i)
    {
        if (wcscmp(argv[i], L"--checkpoint") != 0)
            continue;

        if (i + 1 >= argc)
            break;

        _startupCheckpointPath = fs::path(argv[i + 1]);
        break;
    }

    LocalFree(argv);
}

void MainApp::Try_ApplyStartupCheckpoint()
{
    if (!_hasStartupCheckpoint || _startupCheckpointApplied)
        return;

    if (GAME->Current_LevelIndex() != ETOI(_startupCheckpoint.levelType))
        return;

    if (Client::RuntimeCheckpointService::Apply_ToCurrentLevel(_startupCheckpoint))
    {
        _startupCheckpointApplied = true;
        LOG_INFO("[GameLauncher] startup checkpoint applied: {}",
            String::ToString(_startupCheckpointPath.wstring()));
    }
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

Unique<MainApp> MainApp::Create(const wstring& commandLine)
{
    Unique<MainApp> pInstance{ new MainApp() };

    if (FAILED(pInstance->Initialize(commandLine)))
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

    ClientInstance::DestroyInstance();

    _context.Reset();
    _device.Reset();

    GameInstance::DestroyInstance();
}

NS_END
