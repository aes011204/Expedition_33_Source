#include "pch.h"
#include "MainEditorApp.h"

#include "ClientInstance.h"
#include "EditorInstance.h"
#include "GameInstance.h"
#include "Level_Loading.h"
#include "Profile_View.h"
#include "ResourceLoader.h"

MainEditorApp::MainEditorApp()
{
}

HRESULT MainEditorApp::Initialize()
{
    const std::filesystem::path assetRootPath(Get_ClientResourceRoot());
    if (!std::filesystem::exists(assetRootPath) || !std::filesystem::is_directory(assetRootPath))
        return E_FAIL;

    RECT clientRect {};
    if (!GetClientRect(g_hWnd, &clientRect))
        return E_FAIL;

    const uint32 viewportWidth = max(1L, clientRect.right - clientRect.left);
    const uint32 viewportHeight = max(1L, clientRect.bottom - clientRect.top);

    ENGINE_DESC engineDesc {};
    engineDesc.hInstance = g_hInst;
    engineDesc.hWnd = g_hWnd;
    engineDesc.winMode = WinMode::Win;
    engineDesc.numLevels = ETOI(LevelType::END);
    engineDesc.viewportWidth = viewportWidth;
    engineDesc.viewportHeight = viewportHeight;
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
    editorDesc.iViewportWidth = viewportWidth;
    editorDesc.iViewportHeight = viewportHeight;

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
    {
        Profile_View::Scope profileScope("Editor::Update");
        EDITOR->Update_Editor(timeDelta);
    }

	float runtimeDelta = 0.f;
	float freeCameraDelta = 0.f;

    if (EDITOR->IsPlaying())
    {
        runtimeDelta = timeDelta * EDITOR->Get_RuntimeTimeScale();
		freeCameraDelta = 0.f;
    }
	else if (EDITOR->IsPaused())
	{
		// 1프레임 넘기기
        if (EDITOR->Consume_FrameStep())
            runtimeDelta = timeDelta * EDITOR->Get_RuntimeTimeScale();
        else
            runtimeDelta = 0.f;

        freeCameraDelta = timeDelta;
	}
	else
    {
        runtimeDelta = 0.f;
        freeCameraDelta = timeDelta;
    }

	GAME->Set_FreeCameraTimeDelta(freeCameraDelta);
    {
        Profile_View::Scope profileScope("Game::Update");
        GAME->Update_Engine(runtimeDelta);
    }

	EDITOR->Apply_RuntimeInputPolicy();
}

void MainEditorApp::Late_Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

HRESULT MainEditorApp::Render()
{
    {
        Profile_View::Scope profileScope("Game::Render");

        const Vec4 clearColor = Vec4(0.f, 0.f, 1.f, 1.f);
        if (FAILED(GAME->Clear_Buffers(clearColor)))
            return E_FAIL;

        if (FAILED(GAME->Draw()))
            return E_FAIL;
    }

    {
        Profile_View::Scope profileScope("Editor::Render");

        EDITOR->Capture_BackBuffer_ToViewport();
        GAME->Bind_BackBuffer();
        EDITOR->Render_Editor();
    }

    {
        Profile_View::Scope profileScope("Engine::Present");
        if (FAILED(GAME->Present()))
            return E_FAIL;
    }

    return S_OK;
}

HRESULT MainEditorApp::Ready_Fonts()
{
    const wstring fontPath = Get_ClientResourcePath(L"Fonts/158ex.spritefont");
    if (!std::filesystem::exists(fontPath))
        return E_FAIL;

    if (FAILED(GAME->Add_Font(TEXT("Font_Default"), fontPath.c_str())))
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

wstring MainEditorApp::Get_ClientResourceRoot() const
{
    wchar_t modulePath[MAX_PATH] = {};
    if (0 == GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
        return {};

    // 클라이언트 리소스 루트
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
    auto instance = make_unique<MainEditorApp>();

    if (FAILED(instance->Initialize()))
    {
        MSG_BOX("Failed to Created : MainEditorApp");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void MainEditorApp::Free()
{
    __super::Free();

    EDITOR->Release_Editor();
    EditorInstance::DestroyInstance();

    CLIENT->Release_Client();
    ClientInstance::DestroyInstance();

    _context.Reset();
    _device.Reset();

    GAME->Release_Engine();
    GameInstance::DestroyInstance();
}
