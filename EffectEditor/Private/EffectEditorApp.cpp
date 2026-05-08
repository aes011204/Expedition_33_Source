#include "pch.h"
#include "EffectEditorApp.h"

#include "ClientInstance.h"
#include "EffectEditorInstance.h"
#include "GameInstance.h"
#include "Level_EffectEditor.h"
#include "Level_Loading.h"
#include "Profile_View.h"
#include "ResourceLoader.h"

NS_BEGIN(EffectEditor)

EffectEditorApp::EffectEditorApp()
{
}

HRESULT EffectEditorApp::Initialize()
{
    const fs::path assetRootPath(Get_ClientResourceRoot());
    if (!fs::exists(assetRootPath) || !fs::is_directory(assetRootPath))
        return E_FAIL;

    RECT clientRect{};
    if (!GetClientRect(g_hWnd, &clientRect))
        return E_FAIL;

    const uint32 viewportWidth = max(1L, clientRect.right - clientRect.left);
    const uint32 viewportHeight = max(1L, clientRect.bottom - clientRect.top);

    ENGINE_DESC engineDesc{};
    engineDesc.hInstance = g_hInst;
    engineDesc.hWnd = g_hWnd;
    engineDesc.winMode = WinMode::Win;
    engineDesc.numLevels = ETOI(LevelType::END);
    engineDesc.viewportWidth = viewportWidth;
    engineDesc.viewportHeight = viewportHeight;
    engineDesc.projectName = "EffectEditor";

    CHECK_FAILED(GAME->Initialize_Engine(engineDesc, _device, _context), E_FAIL);
    CHECK_FAILED(GAME->Initialize_AssetManager(assetRootPath.wstring()), E_FAIL);
    CHECK_FAILED(CLIENT->Initialize_Client(_device, _context), E_FAIL);
    CHECK_FAILED(Ready_Fonts(), E_FAIL);
    CHECK_FAILED(Ready_Prototype_For_Static_Level(), E_FAIL);
    CHECK_FAILED(CLIENT->Ready_LevelResources(LevelType::GamePlay), E_FAIL);
    CHECK_FAILED(Ready_PreviewLevel(), E_FAIL);

    EffectEditorDesc editorDesc{};
    editorDesc.hWnd = g_hWnd;
    editorDesc.winMode = WinMode::Win;
    editorDesc.viewportWidth = viewportWidth;
    editorDesc.viewportHeight = viewportHeight;

    CHECK_FAILED(EDITOR->Initialize_Editor(editorDesc, _device, _context), E_FAIL);

    return S_OK;
}

void EffectEditorApp::Priority_Update(float)
{
}

void EffectEditorApp::Update(float timeDelta)
{
    {
        Profile_View::Scope profileScope("Editor::Update");
        EDITOR->Update_Editor(timeDelta);
    }

    float runtimeDelta;
    float freeCameraDelta;

    if (EDITOR->IsPlaying())
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
        Profile_View::Scope profileScope("Game::Update");
        GAME->Update_Engine(runtimeDelta);
    }

    EDITOR->Apply_RuntimeInputPolicy();
}

void EffectEditorApp::Late_Update(float)
{
}

HRESULT EffectEditorApp::Render()
{
    {
        Profile_View::Scope profileScope("Game::Render");

        if (FAILED(GAME->Clear_Buffers(defaultClearColor)))
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

HRESULT EffectEditorApp::Ready_PreviewLevel()
{
    // preview level은 저장 level이 아니므로 GamePlay resource index를 명시적으로 빌려 쓴다.
    const HRESULT hr = GAME->Change_Level(
        ETOI(LevelType::GamePlay),
        Level_EffectEditor::Create(_device, _context)
    );
    CHECK_FAILED(hr, E_FAIL);

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
    auto instance = make_unique<EffectEditorApp>();

    if (FAILED(instance->Initialize()))
    {
        LOG_CRITICAL("Failed to Create : EffectEditorApp");
        MSG_BOX("Failed to Create : EffectEditorApp");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void EffectEditorApp::Free()
{
    __super::Free();

    EffectEditorInstance::DestroyInstance();
    ClientInstance::DestroyInstance();

    _context.Reset();
    _device.Reset();

    GameInstance::DestroyInstance();
}

NS_END
