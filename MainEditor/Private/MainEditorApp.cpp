#include "pch.h"
#include "MainEditorApp.h"

#include "ClientInstance.h"
#include "EditorInstance.h"
#include "GameInstance.h"
#include "Level_Loading.h"
#include "ResourceLoader.h"

#include <filesystem>

MainEditorApp::MainEditorApp()
    : _gameInstance{ GameInstance::GetInstance() }
{
}

HRESULT MainEditorApp::Initialize()
{
    const std::filesystem::path assetRootPath(Get_ClientResourceRoot());
    if (!std::filesystem::exists(assetRootPath) || !std::filesystem::is_directory(assetRootPath))
        return E_FAIL;

    ENGINE_DESC engineDesc{};
    engineDesc.hInstance = g_hInst;
    engineDesc.hWnd = g_hWnd;
    engineDesc.winMode = WinMode::Win;
    engineDesc.numLevels = ETOI(LevelType::END);
    engineDesc.viewportWidth = g_iWinSizeX;
    engineDesc.viewportHeight = g_iWinSizeY;
    engineDesc.projectName = "MainEditor";

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

    EDITOR_DESC editorDesc{};
    editorDesc.hWnd = g_hWnd;
    editorDesc.eWinMode = WinMode::Win;
    editorDesc.iViewportWidth = g_iWinSizeX;
    editorDesc.iViewportHeight = g_iWinSizeY;

    if (FAILED(EDITOR->Initialize_Editor(editorDesc, _device, _context)))
        return E_FAIL;

    if (FAILED(_gameInstance->Add_Timer(TEXT("Timer_Default"))))
        return E_FAIL;

    if (FAILED(_gameInstance->Add_Timer(TEXT("Timer_60"))))
        return E_FAIL;

    return S_OK;
}

void MainEditorApp::Priority_Update(float)
{
}

void MainEditorApp::Update(float fTimeDelta)
{
    _gameInstance->Update_Engine(fTimeDelta);
    EDITOR->Update_Editor(fTimeDelta);
}

void MainEditorApp::Late_Update(float)
{
}

HRESULT MainEditorApp::Render()
{
    EDITOR->Render_RuntimeViewport();

    const Vec4 clearColor = Vec4(0.f, 0.f, 1.f, 1.f);
    if (FAILED(_gameInstance->Clear_Buffers(clearColor)))
        return E_FAIL;

    EDITOR->Render_Editor();

    if (FAILED(_gameInstance->Present()))
        return E_FAIL;

    return S_OK;
}

HRESULT MainEditorApp::Ready_Fonts()
{
    const wstring fontPath = Get_ClientResourcePath(L"Fonts/158ex.spritefont");
    if (!std::filesystem::exists(fontPath))
        return E_FAIL;

    if (FAILED(_gameInstance->Add_Font(TEXT("Font_Default"), fontPath.c_str())))
        return E_FAIL;

    return S_OK;
}

HRESULT MainEditorApp::Ready_Prototype_For_Static_Level()
{
    return CLIENT->Ready_StaticResources();
}

HRESULT MainEditorApp::Ready_StartLevel(LevelType eStartLevelID)
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

wstring MainEditorApp::Get_ClientResourceRoot() const
{
    wchar_t modulePath[MAX_PATH] = {};
    if (0 == GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
        return {};

    const std::filesystem::path runtimeRoot = std::filesystem::path(modulePath).parent_path();
    return std::filesystem::absolute(runtimeRoot / L"../../../Client/Bin/Resources").lexically_normal().wstring();
}

wstring MainEditorApp::Get_ClientResourcePath(const wchar_t* relativePath) const
{
    if (nullptr == relativePath || L'\0' == *relativePath)
        return Get_ClientResourceRoot();

    return (std::filesystem::path(Get_ClientResourceRoot()) / relativePath).lexically_normal().wstring();
}

Unique<MainEditorApp> MainEditorApp::Create()
{
    Unique<MainEditorApp> pInstance{ new MainEditorApp() };

    if (FAILED(pInstance->Initialize()))
    {
        MSG_BOX("Failed to Created : MainEditorApp");
        pInstance->Free();
        return nullptr;
    }

    return pInstance;
}

void MainEditorApp::Free()
{
    __super::Free();

    EditorInstance::DestroyInstance();

    CLIENT->Release_Client();
    ClientInstance::DestroyInstance();

    if (nullptr != _gameInstance)
        _gameInstance->Release_Engine();
}
