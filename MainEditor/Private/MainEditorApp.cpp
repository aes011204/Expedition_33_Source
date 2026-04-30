#include "pch.h"
#include "MainEditorApp.h"

#include "ClientInstance.h"
#include "EditorInstance.h"
#include "GameInstance.h"
#include "Level_Loading.h"
#include "ResourceLoader.h"

MainEditorApp::MainEditorApp()
    : _gameInstance{ GameInstance::GetInstance() }
{
}

HRESULT MainEditorApp::Initialize()
{
    ENGINE_DESC engineDesc{};
    engineDesc.hInstance = g_hInst;
    engineDesc.hWnd = g_hWnd;
    engineDesc.eWinMode = WinMode::Win;
    engineDesc.iNumLevels = ETOI(LevelType::END);
    engineDesc.iViewportWidth = g_iWinSizeX;
    engineDesc.iViewportHeight = g_iWinSizeY;

    if (FAILED(GAME->Initialize_Engine(engineDesc, _device, _context)))
        return E_FAIL;

    if (FAILED(GAME->Initialize_AssetManager(TEXT("../Bin/Resources"))))
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

void MainEditorApp::Priority_Update(float fTimeDelta)
{
    UNREFERENCED_PARAMETER(fTimeDelta);
}

void MainEditorApp::Update(float fTimeDelta)
{
    _gameInstance->Update_Engine(fTimeDelta);
    EDITOR->Update_Editor(fTimeDelta);
}

void MainEditorApp::Late_Update(float fTimeDelta)
{
    UNREFERENCED_PARAMETER(fTimeDelta);
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
    if (FAILED(_gameInstance->Add_Font(TEXT("Font_Default"), TEXT("../Bin/Resources/Fonts/158ex.spritefont"))))
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
