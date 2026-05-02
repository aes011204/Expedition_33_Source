#include "pch.h"
#include "EffectEditorApp.h"

#include "ClientInstance.h"
#include "EffectEditorInstance.h"
#include "GameInstance.h"
#include "Level_Loading.h"
#include "ResourceLoader.h"

EffectEditorApp::EffectEditorApp()
{
}

EffectEditorApp::~EffectEditorApp()
{
    Free();
}

HRESULT EffectEditorApp::Initialize()
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
    engineDesc.projectName = "EffectEditor";

    CHECK_FAILED(GAME->Initialize_Engine(engineDesc, _device, _context), E_FAIL);

    CHECK_FAILED(GAME->Initialize_AssetManager(assetRootPath.wstring()), E_FAIL);

    CHECK_FAILED(CLIENT->Initialize_Client(_device, _context), E_FAIL);

    CHECK_FAILED(Ready_Fonts(), E_FAIL);

    CHECK_FAILED(Ready_Prototype_For_Static_Level(), E_FAIL);

    CHECK_FAILED(Ready_StartLevel(LevelType::Logo), E_FAIL);

    EffectEditorDesc editorDesc{};
    editorDesc.hWnd = g_hWnd;
    editorDesc.eWinMode = WinMode::Win;
    editorDesc.iViewportWidth = g_iWinSizeX;
    editorDesc.iViewportHeight = g_iWinSizeY;

    CHECK_FAILED(EDITOR->Initialize_Editor(editorDesc, _device, _context), E_FAIL);

    return S_OK;
}

void EffectEditorApp::Priority_Update(float)
{
}

void EffectEditorApp::Update(float timeDelta)
{
    GAME->Update_Engine(timeDelta);
    EDITOR->Update_Editor(timeDelta);
}

void EffectEditorApp::Late_Update(float)
{
}

HRESULT EffectEditorApp::Render()
{
    const Vec4 clearColor = Vec4(0.f, 0.f, 1.f, 1.f);

    CHECK_FAILED(GAME->Clear_Buffers(clearColor), E_FAIL);

    CHECK_FAILED(GAME->Draw(), E_FAIL);

    EDITOR->Capture_BackBuffer_ToViewport();

    GAME->Bind_BackBuffer();

    EDITOR->Render_Editor();

    if (FAILED(GAME->Present()))
        return E_FAIL;

    return S_OK;
}

HRESULT EffectEditorApp::Ready_Fonts()
{
    const wstring fontPath = Get_ClientResourcePath(L"Fonts/158ex.spritefont");
    if (!fs::exists(fontPath))
        return E_FAIL;

    CHECK_FAILED(GAME->Add_Font(TEXT("Font_Default"), fontPath.c_str()), E_FAIL);

    return S_OK;
}

HRESULT EffectEditorApp::Ready_Prototype_For_Static_Level()
{
    return CLIENT->Ready_StaticResources();
}

HRESULT EffectEditorApp::Ready_StartLevel(LevelType startLevelID)
{
    if (LevelType::Loading == startLevelID)
        return E_FAIL;

    if (FAILED(
        GAME->Change_Level(
            ETOI(LevelType::Loading),
            Level_Loading::Create(_device.Get(), _context.Get(), startLevelID))
    ))
        return E_FAIL;

    return S_OK;
}

wstring EffectEditorApp::Get_ClientResourceRoot() const
{
    wchar_t modulePath[MAX_PATH] = {};
    if (0 == GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
        return {};

    const fs::path runtimeRoot = fs::path(modulePath).parent_path();
    return fs::absolute(runtimeRoot / L"../../../Client/Bin/Resources").lexically_normal().wstring();
}

wstring EffectEditorApp::Get_ClientResourcePath(const wchar_t* relativePath) const
{
    if (nullptr == relativePath || L'\0' == *relativePath)
        return Get_ClientResourceRoot();

    return (fs::path(Get_ClientResourceRoot()) / relativePath).lexically_normal().wstring();
}

Unique<EffectEditorApp> EffectEditorApp::Create()
{
    Unique<EffectEditorApp> pInstance{ new EffectEditorApp() };

    if (FAILED(pInstance->Initialize()))
    {
        MSG_BOX("Failed to Created : EffectEditorApp");
        pInstance->Free();
        return nullptr;
    }

    return pInstance;
}

void EffectEditorApp::Free()
{
    EffectEditorInstance::DestroyInstance();

    CLIENT->Release_Client();
    ClientInstance::DestroyInstance();

    if (nullptr != GAME)
        GAME->Release_Engine();
}
