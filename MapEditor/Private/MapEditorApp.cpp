#include "pch.h"
#include "MapEditorApp.h"
#include "GameInstance.h"
#include "RenderTarget.h"

#include "MEImGuiManager.h"
#include "MEManager.h"
#include "MECamera.h"
#include "PanelViewport.h"


extern HWND g_hWnd;
extern HINSTANCE g_hInst;

MapEditorApp::~MapEditorApp()
{

}

HRESULT MapEditor::MapEditorApp::Initialize()
{
    ENGINE_DESC desc{};
    desc.hInstance = g_hInst;
    desc.hWnd = g_hWnd;
    desc.winMode = WinMode::Win;
    desc.numLevels = 1;
    desc.viewportWidth = g_iWinSizeX;
    desc.viewportHeight = g_iWinSizeY;
    desc.projectName = "MapEditor";

    CHECK_FAILED(GAME->Initialize_Engine(desc, _device, _context), E_FAIL);

    _imguiManager = MEImGuiManager::Create(g_hWnd, _device, _context);
    CHECK_NULL(_imguiManager, E_FAIL);

    _camera = MECamera::Create(60.f, static_cast<float>(g_iWinSizeX) / g_iWinSizeY, 0.1f, 1000.f);

    _rt = RenderTarget::Create(_device, _context, g_iWinSizeX, g_iWinSizeY, DXGI_FORMAT_R8G8B8A8_UNORM, Vec4(0.1f, 0.1f, 0.1f, 1.f));
    CHECK_NULL(_rt, E_FAIL);

    _viewport = PanelViewport::Create(_camera, _rt, &_scene);
    CHECK_NULL(_viewport, E_FAIL);

    _manager = MEManager::Create(_viewport, &_scene);
    CHECK_NULL(_manager, E_FAIL);

    return S_OK;
}

void MapEditor::MapEditorApp::Priority_Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void MapEditor::MapEditorApp::Update(float timeDelta)
{
    GAME->Update_Engine(timeDelta);
    _imguiManager->Update(timeDelta);
    _manager->Update(timeDelta);
}

void MapEditor::MapEditorApp::Late_Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

HRESULT MapEditor::MapEditorApp::Render()
{
    _camera->Apply();

    CHECK_FAILED(_rt->Begin(), E_FAIL);
    CHECK_FAILED(GAME->Draw(), E_FAIL);
    GAME->Bind_BackBuffer();

    CHECK_FAILED(GAME->Clear_Buffers(Vec4(0.1f, 0.1f, 0.1f, 1.f)), E_FAIL);

    GAME->Bind_BackBuffer();

    _manager->Render();
    _imguiManager->Render();

    CHECK_FAILED(GAME->Present(), E_FAIL);

    return S_OK;
}

Unique<MapEditor::MapEditorApp> MapEditor::MapEditorApp::Create()
{
    Unique<MapEditorApp> instance{ new MapEditorApp() };

    if (FAILED(instance->Initialize()))
    {
        MSG_BOX("Failed to Create MapEditorApp");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void MapEditor::MapEditorApp::Free()
{
    __super::Free();

    _viewport.reset();
    _rt.reset();
    _camera.reset();
    _manager.reset();
    _imguiManager.reset();
    _context.Reset();
    _device.Reset();

    if (GAME)
    {
        GAME->Release_Engine();
    }
}
