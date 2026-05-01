#include "pch.h"
#include "MainEditorApp.h"

#include "ClientInstance.h"
#include "EditorInstance.h"
#include "GameInstance.h"
#include "Level_Loading.h"
#include "ResourceLoader.h"

MainEditorApp::MainEditorApp()
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


    return S_OK;
}

void MainEditorApp::Priority_Update(float timeDelta)
{
    // 안쓴다는 매크로임
    UNREFERENCED_PARAMETER(timeDelta);
}

void MainEditorApp::Update(float timeDelta)
{
    GAME->Update_Engine(timeDelta);
    EDITOR->Update_Editor(timeDelta);
}

void MainEditorApp::Late_Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

HRESULT MainEditorApp::Render()
{
    const Vec4 clearColor = Vec4(0.f, 0.f, 1.f, 1.f);
    if (FAILED(GAME->Clear_Buffers(clearColor)))
        return E_FAIL;

    if (FAILED(GAME->Draw()))
        return E_FAIL;

    EDITOR->Capture_BackBuffer_ToViewport();

    GAME->Bind_BackBuffer();          

    EDITOR->Render_Editor();

    if (FAILED(GAME->Present()))
        return E_FAIL;

    return S_OK;
}

HRESULT MainEditorApp::Ready_Fonts()
{
    if (FAILED(GAME->Add_Font(TEXT("Font_Default"), TEXT("../Bin/Resources/Fonts/158ex.spritefont"))))
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

    if (FAILED(GAME->Change_Level(
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

    if (nullptr != GAME)
        GAME->Release_Engine();
}
