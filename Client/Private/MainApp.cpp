#include "MainApp.h"

#include "GameInstance.h"
#include "ClientInstance.h"

#include "Level_Loading.h"
#include "ResourceLoader.h"

namespace fs = std::filesystem;

MainApp::MainApp()
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
    const fs::path assetRootPath(Get_ClientResourceRoot());
    if (!fs::exists(assetRootPath) || !fs::is_directory(assetRootPath))
        return E_FAIL;

    /* 엔진을 이용하기위해 엔진프로젝트를 준비시킨다. */
    ENGINE_DESC		EngineDesc{};
    EngineDesc.hInstance = g_hInst;
    EngineDesc.hWnd = g_hWnd;
    EngineDesc.winMode = WinMode::Win;
    EngineDesc.numLevels = ETOI(LevelType::END);
    EngineDesc.viewportWidth = g_iWinSizeX;
    EngineDesc.viewportHeight = g_iWinSizeY;
    EngineDesc.projectName = "Client";

    if (FAILED(GAME->Initialize_Engine(EngineDesc, _device, _context)))
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
    GAME->Update_Engine(fTimeDelta);

}

HRESULT MainApp::Render()
{
    Vec4			clearColor = Vec4(0.f, 0.f, 1.f, 1.f);
    if (FAILED(GAME->Clear_Buffers(clearColor)))
        return E_FAIL;

    if (FAILED(GAME->Draw()))
        return E_FAIL;

    GAME->Draw_Font(TEXT("Font_Default"), TEXT("니네들은 싸우지마, 욕하지마!"), Vec2(100.f, 0.f));

    if (FAILED(GAME->Present()))
        return E_FAIL;

    return S_OK;
}

HRESULT MainApp::Ready_Fonts()
{
    /*MakeSpriteFont "넥슨lv1고딕 Bold" /FontSize:16 /FastPack /CharacterRegion:0x0020-0x00FF /CharacterRegion:0x3131-0x3163 /CharacterRegion:0xAC00-0xD800 /DefaultCharacter:0xAC00 158ex.spritefont */

    const wstring fontPath = Get_ClientResourcePath(L"Fonts/158ex.spritefont");
    if (!fs::exists(fontPath))
        return E_FAIL;

    if (FAILED(GAME->Add_Font(TEXT("Font_Default"), fontPath.c_str())))
        return E_FAIL;

    return S_OK;
}

wstring MainApp::Get_ClientResourceRoot() const
{
    wchar_t modulePath[MAX_PATH] = {};
    if (0 == GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
        return {};

    const fs::path runtimeRoot = fs::path(modulePath).parent_path();
    return fs::absolute(runtimeRoot / L"../Resources").lexically_normal().wstring();
}

wstring MainApp::Get_ClientResourcePath(const wchar_t* relativePath) const
{
    if (nullptr == relativePath || L'\0' == *relativePath)
        return Get_ClientResourceRoot();

    return (fs::path(Get_ClientResourceRoot()) / relativePath).lexically_normal().wstring();
}

HRESULT MainApp::Ready_Prototype_For_Static_Level()
{
    return CLIENT->Ready_StaticResources();
}

HRESULT MainApp::Ready_StartLevel(LevelType eStartLevelID)
{
    if (LevelType::Loading == eStartLevelID)
        return E_FAIL;

    if (FAILED(GAME->Change_Level(ETOI(LevelType::Loading), Level_Loading::Create(_device.Get(), _context.Get(), eStartLevelID))))
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

    _context.Reset();
    _device.Reset();

    GAME->Release_Engine();
    GameInstance::DestroyInstance();
}
