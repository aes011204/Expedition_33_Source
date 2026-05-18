#include "pch.h"
#include "EffectEditorApp.h"

#include <chrono>
#include "ClientInstance.h"
#include "EffectEditorInstance.h"
#include "GameInstance.h"
#include "Level_EffectEditor.h"
#include "PostProcessPipeline.h"
#include "Profile_View.h"
#include "ResourceLoader.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
    bool Starts_With(const wstring& value, const wchar_t* prefix)
    {
        const size_t prefixLength = wcslen(prefix);
        return value.size() >= prefixLength && 0 == value.compare(0, prefixLength, prefix);
    }

    bool Is_EffectEditorStaticLevelAllowed(const wstring& prototypeTag)
    {
        return prototypeTag == L"VIBuffer_Rect" || prototypeTag == L"VIBuffer_Cube";
    }

    bool Is_EffectEditorStaticLoadJobAllowed(const FLoadJob& loadJob)
    {
        switch (loadJob.eType)
        {
        case ELoadJobType::Shader:
        case ELoadJobType::ComputeShader:
            return true;

        case ELoadJobType::Component:
            return Starts_With(loadJob.strPrototypeTag, L"Com_Effect_");

        case ELoadJobType::Model:
            return Starts_With(loadJob.strPrototypeTag, L"Model_Effect_");

        case ELoadJobType::GameObject:
            return Starts_With(loadJob.strPrototypeTag, L"GameObject_Effect_");

        case ELoadJobType::StaticLevel:
            return Is_EffectEditorStaticLevelAllowed(loadJob.strPrototypeTag);

        case ELoadJobType::TextureCreate:
            return Starts_With(loadJob.strPrototypeTag, L"Texture_Effect_");

        default:
            return false;
        }
    }

    void Log_EffectEditorStaticLoadSummary(size_t totalCount, size_t includedCount, size_t excludedCount, size_t failedCount)
    {
        LOG_INFO(
            "[EffectEditorStaticFilter] total={}, included={}, excluded={}, failed={}",
            static_cast<uint32>(totalCount),
            static_cast<uint32>(includedCount),
            static_cast<uint32>(excludedCount),
            static_cast<uint32>(failedCount)
        );
    }
}

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

    const auto startupStartTime = chrono::steady_clock::now();

    long long initializeEngineMs = 0;
    long long initializeAssetManagerMs = 0;
    long long initializeClientMs = 0;
    long long readyFontsMs = 0;
    long long readyStaticResourcesMs = 0;
    long long readyGamePlayResourcesMs = 0;
    long long readyPreviewLevelMs = 0;
    long long initializeEditorMs = 0;

    const auto run_startup_step = [](long long& elapsedMs, const auto& step) -> HRESULT
    {
        const auto startTime = chrono::steady_clock::now();
        const HRESULT hr = step();
        elapsedMs = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - startTime).count();

        return hr;
    };

    CHECK_FAILED(run_startup_step(initializeEngineMs, [&]{ return GAME->Initialize_Engine(engineDesc, _device, _context); }), E_FAIL);
    CHECK_FAILED(run_startup_step(initializeAssetManagerMs, [&]{ return GAME->Initialize_AssetManager(assetRootPath.wstring()); }), E_FAIL);
    CHECK_FAILED(run_startup_step(initializeClientMs, [&]{ return CLIENT->Initialize_Client(_device, _context); }), E_FAIL);
    CHECK_FAILED(run_startup_step(readyFontsMs, [&]{ return Ready_Fonts(); }), E_FAIL);
    CHECK_FAILED(run_startup_step(readyStaticResourcesMs, [&]{ return Ready_Prototype_For_Static_Level(); }), E_FAIL);
    CHECK_FAILED(run_startup_step(readyGamePlayResourcesMs, [&]{ return CLIENT->Ready_LevelResources(LevelType::GamePlay); }), E_FAIL);
    CHECK_FAILED(run_startup_step(readyPreviewLevelMs, [&]{ return Ready_PreviewLevel(); }), E_FAIL);

    EffectEditorDesc editorDesc{};
    editorDesc.hWnd = g_hWnd;
    editorDesc.winMode = WinMode::Win;
    editorDesc.viewportWidth = viewportWidth;
    editorDesc.viewportHeight = viewportHeight;

    CHECK_FAILED(run_startup_step(initializeEditorMs, [&]{ return EDITOR->Initialize_Editor(editorDesc, _device, _context); }), E_FAIL);
    EDITOR->Request_RestartPreview();

    const auto totalStartupMs = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - startupStartTime).count();
    LOG_INFO(
        "EffectEditor startup summary. total={}ms engine={}ms assetManager={}ms client={}ms fonts={}ms staticResources={}ms gameplayResources={}ms previewLevel={}ms editor={}ms",
        totalStartupMs,
        initializeEngineMs,
        initializeAssetManagerMs,
        initializeClientMs,
        readyFontsMs,
        readyStaticResourcesMs,
        readyGamePlayResourcesMs,
        readyPreviewLevelMs,
        initializeEditorMs
    );

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
    // EFFECTEDITOR_BG_FALLBACK_TEMP: Draw 스코프 밖에서 복구할 수 있도록 사전 백업한다.
    Shared<PostProcessPipeline> postProcessPipeline = GAME->Get_PostProcessPipeline();
    PostProcessCB originalPostProcessParams{};
    bool hasOriginalPostProcessParams = false;
    if (postProcessPipeline != nullptr)
    {
        originalPostProcessParams = postProcessPipeline->Get_PostProcessParams();
        hasOriginalPostProcessParams = true;
    }

    // EFFECTEDITOR_BG_FALLBACK_TEMP: EffectEditor에서 Play가 아닐 때를 Authoring 관측 모드로 보고 fallback을 임시 적용한다.
    const bool useAuthoringBackgroundFallback = !EDITOR->IsPlaying();
    if (hasOriginalPostProcessParams && useAuthoringBackgroundFallback)
    {
        PostProcessCB authoringPostProcessParams = originalPostProcessParams;
        authoringPostProcessParams.useBackgroundFallback = 1;
        // EFFECTEDITOR_BG_FALLBACK_TEMP: authoring fallback 가시성을 위해 임시 배경색을 0.3으로 올린다.
        authoringPostProcessParams.backgroundFallbackColor = Vec4(0.3f, 0.3f, 0.3f, 1.f);
        postProcessPipeline->Set_PostProcessParams(authoringPostProcessParams);
    }

    {
        Profile_View::Scope profileScope("Game::Render");

        if (FAILED(GAME->Clear_Buffers(kDefaultClearColor)))
        {
            // EFFECTEDITOR_BG_FALLBACK_TEMP: 조기 반환 경로에서도 원복을 보장한다.
            if (hasOriginalPostProcessParams)
                postProcessPipeline->Set_PostProcessParams(originalPostProcessParams);
            return E_FAIL;
        }

        if (FAILED(GAME->Draw()))
        {
            // EFFECTEDITOR_BG_FALLBACK_TEMP: 조기 반환 경로에서도 원복을 보장한다.
            if (hasOriginalPostProcessParams)
                postProcessPipeline->Set_PostProcessParams(originalPostProcessParams);
            return E_FAIL;
        }
    }

    // EFFECTEDITOR_BG_FALLBACK_TEMP: Draw 이후 postprocess 파라미터를 항상 원본으로 복구한다.
    if (hasOriginalPostProcessParams)
        postProcessPipeline->Set_PostProcessParams(originalPostProcessParams);

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
    const Shared<ResourceLoader> resourceLoader = CLIENT->Get_ResourceLoader();
    CHECK_NULL(resourceLoader, E_FAIL);

    vector<FLoadJob> loadJobs;
    CHECK_FAILED(resourceLoader->Build_LevelJobs(LevelType::Static, loadJobs), E_FAIL);

    size_t includedCount = 0;
    size_t excludedCount = 0;
    size_t failedCount = 0;

    for (const FLoadJob& loadJob : loadJobs)
    {
        const bool included = Internal::Is_EffectEditorStaticLoadJobAllowed(loadJob);

        if (!included)
        {
            ++excludedCount;
            continue;
        }

        ++includedCount;

        if (FAILED(resourceLoader->Register_LoadJob(loadJob)))
        {
            ++failedCount;
            LOG_WARN(
                "[EffectEditorStaticFilter] register failed. type={}, tag={}",
                string(magic_enum::enum_name(loadJob.eType)),
                String::ToString(loadJob.strPrototypeTag)
            );
            continue;
        }
    }

    Internal::Log_EffectEditorStaticLoadSummary(loadJobs.size(), includedCount, excludedCount, failedCount);

    return S_OK;
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
