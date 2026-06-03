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
    const auto startupBegin = chrono::steady_clock::now();
    auto stepBegin = startupBegin;

    const std::filesystem::path assetRootPath(Get_ClientResourceRoot());
    if (!std::filesystem::exists(assetRootPath) || !std::filesystem::is_directory(assetRootPath))
        return E_FAIL;
    const auto resourceRootElapsed = chrono::duration_cast<chrono::milliseconds>(
        chrono::steady_clock::now() - stepBegin).count();

    stepBegin = chrono::steady_clock::now();
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
    engineDesc.swapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    engineDesc.projectName = "MainEditor";

    if (FAILED(GAME->Initialize_Engine(engineDesc, _device, _context)))
        return E_FAIL;
    LOG_INFO("[StartupTiming] MainEditorApp::ResourceRootCheck elapsed={}ms", resourceRootElapsed);
    LOG_INFO("[StartupTiming] MainEditorApp::Initialize_Engine elapsed={}ms",
        chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - stepBegin).count());

    stepBegin = chrono::steady_clock::now();
    if (FAILED(GAME->Initialize_AssetManager(assetRootPath.wstring())))
        return E_FAIL;
    LOG_INFO("[StartupTiming] MainEditorApp::Initialize_AssetManager elapsed={}ms",
        chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - stepBegin).count());

    stepBegin = chrono::steady_clock::now();
    if (FAILED(CLIENT->Initialize_Client(_device, _context)))
        return E_FAIL;
    LOG_INFO("[StartupTiming] MainEditorApp::Initialize_Client elapsed={}ms",
        chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - stepBegin).count());

    stepBegin = chrono::steady_clock::now();
    if (FAILED(Ready_Fonts()))
        return E_FAIL;
    LOG_INFO("[StartupTiming] MainEditorApp::Ready_Fonts elapsed={}ms",
        chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - stepBegin).count());

    stepBegin = chrono::steady_clock::now();
    if (FAILED(Ready_StartLevel(LevelType::MainTitle)))
        return E_FAIL;
    LOG_INFO("[StartupTiming] MainEditorApp::Ready_StartLevel elapsed={}ms",
        chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - stepBegin).count());

    EDITOR_DESC editorDesc{};
    editorDesc.hWnd = g_hWnd;
    editorDesc.eWinMode = WinMode::Win;
    editorDesc.iViewportWidth = viewportWidth;
    editorDesc.iViewportHeight = viewportHeight;

    stepBegin = chrono::steady_clock::now();
    if (FAILED(EDITOR->Initialize_Editor(editorDesc, _device, _context)))
        return E_FAIL;
    LOG_INFO("[StartupTiming] MainEditorApp::Initialize_Editor elapsed={}ms",
        chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - stepBegin).count());

    LOG_INFO("[StartupTiming] MainEditorApp::Initialize totalElapsed={}ms",
        chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - startupBegin).count());

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
        ENGINE_PROFILE_SCOPE_BASIC("Editor::Update");
        EDITOR->Update_Editor(timeDelta);
    }

    float runtimeDelta = 0.f;
    float freeCameraDelta = 0.f;

    const uint32 previousLevelIndex = GAME->Current_LevelIndex();
    const bool isLoading = previousLevelIndex == ETOI(LevelType::Loading);

    if (isLoading)
    {
        runtimeDelta = timeDelta;
        freeCameraDelta = 0.f;
    }
    else if (EDITOR->IsPlaying())
    {
        runtimeDelta = timeDelta * EDITOR->Get_RuntimeTimeScale();
        freeCameraDelta = 0.f;
    }
    else if (EDITOR->IsPaused())
    {
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
        ENGINE_PROFILE_SCOPE_BASIC("Game::Update");

        GAME->Update_Engine(
            runtimeDelta,
            [](float dt)
            {
                if (dt > 0.f)
                    CLIENT->Update_Client(dt);
            });
    }

    const uint32 currentLevelIndex = GAME->Current_LevelIndex();
    const bool finishedLoadingIntoEditorAutoPlayLevel =
        previousLevelIndex == ETOI(LevelType::Loading) &&
        currentLevelIndex != ETOI(LevelType::Loading) &&
        currentLevelIndex != ETOI(LevelType::MainTitle) &&
        currentLevelIndex != ETOI(LevelType::GameEntry);

    if (finishedLoadingIntoEditorAutoPlayLevel)
        EDITOR->Enter_EditAfterLoading();

    EDITOR->Apply_RuntimeInputPolicy();
}


void MainEditorApp::Late_Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

HRESULT MainEditorApp::Render()
{
    auto fail = [&]() -> HRESULT
    {
        EDITOR->EndImGuiFrameIfBegun();
        return E_FAIL;
    };

    {
        ENGINE_PROFILE_SCOPE_BASIC("Game::Render");

        {
            ENGINE_PROFILE_SCOPE_DETAIL("Game::BindBackBuffer");
            GAME->Bind_BackBuffer();
        }

        const bool isLoading = GAME->Current_LevelIndex() == ETOI(LevelType::Loading);
        const Vec4 clearColor = isLoading
            ? Vec4(0.f, 0.f, 0.f, 1.f)
            : Vec4(0.f, 0.f, 1.f, 1.f);

        {
            ENGINE_PROFILE_SCOPE_DETAIL("Game::ClearBuffers");
            if (FAILED(GAME->Clear_Buffers(clearColor)))
                return fail();
        }

        {
            ENGINE_PROFILE_SCOPE_BASIC("Game::Draw");
            if (FAILED(GAME->Draw()))
                return fail();
        }

        // picking
        {
            ENGINE_PROFILE_SCOPE_DETAIL("Editor::FlushPendingPick");
            EDITOR->Flush_PendingPick();
        }
    }

    {
        ENGINE_PROFILE_SCOPE_DETAIL("Editor::CaptureViewport");
        EDITOR->Capture_BackBuffer_ToViewport();
    }

    {
        ENGINE_PROFILE_SCOPE_BASIC("Editor::RenderUI");

        {
            ENGINE_PROFILE_SCOPE_DETAIL("Editor::BindBackBuffer");
            GAME->Bind_BackBuffer();
        }

        {
            ENGINE_PROFILE_SCOPE_DETAIL("Editor::ClearBackBuffer");
            if (FAILED(GAME->Clear_Buffers(Vec4(0.018f, 0.020f, 0.024f, 1.f))))
                return fail();
        }

        {
            ENGINE_PROFILE_SCOPE_DETAIL("Editor::Render_Editor");
            EDITOR->Render_Editor();
        }

        {
            ENGINE_PROFILE_SCOPE_DETAIL("Editor::RestoreBackBuffer");
            GAME->Bind_BackBuffer();
        }
    }

    {
        ENGINE_PROFILE_SCOPE_BASIC("Engine::Present");
        if (FAILED(GAME->Present()))
            return fail();
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

    EditorInstance::DestroyInstance();

    ClientInstance::DestroyInstance();

    _context.Reset();
    _device.Reset();

    GameInstance::DestroyInstance();
}
