#include "pch.h"
#include "EffectEditorApp.h"

#include "ClientInstance.h"
#include "ComputeShaderCom.h"
#include "ComputeStructuredBuffer.h"
#include "EffectEditorInstance.h"
#include "GameInstance.h"
#include "Level_EffectEditor.h"
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

#ifdef _DEBUG
    CHECK_FAILED(Run_ComputeShaderSmokeTest(), E_FAIL);
#endif

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

HRESULT EffectEditorApp::Run_ComputeShaderSmokeTest()
{
    // TODO : Smoke test용. 정식 compute shader는 이후 DT_ComputeShader 같은 authoring table에 등록해 Id로 사용할 예정이다.
    const wstring shaderPath = Get_ClientShaderPath(L"Compute/Shader_Compute_BufferSmoke.hlsl");
    if (!fs::exists(shaderPath))
    {
        LOG_ERROR("Compute smoke shader not found: {}", String::ToString(shaderPath));
        return E_FAIL;
    }

    // 1. CPU에서 검증용 입력 데이터를 준비한다.
    // shader는 각 float4를 2배로 만들어 output buffer에 쓴다.
    const array<Vec4, computeSmokeElementCount> inputData{
        Vec4{ 1.f, 2.f, 3.f, 4.f },
        Vec4{ 2.f, 3.f, 4.f, 5.f },
        Vec4{ 3.f, 4.f, 5.f, 6.f },
        Vec4{ 4.f, 5.f, 6.f, 7.f }
    };

    // 2. input은 SRV(t0)로 읽고, output은 UAV(u0)로 쓸 structured buffer다.
    const auto inputBuffer = ComputeStructuredBuffer::Create(
        _device,
        _context,
        sizeof(Vec4),
        computeSmokeElementCount,
        inputData.data()
    );
    CHECK_NULL(inputBuffer, E_FAIL);

    const auto outputBuffer = ComputeStructuredBuffer::Create(
        _device,
        _context,
        sizeof(Vec4),
        computeSmokeElementCount
    );
    CHECK_NULL(outputBuffer, E_FAIL);

    // 3. compute-only hlsl을 cs_5_0으로 컴파일하고 CS_Main entry를 실행 대상으로 잡는다.
    const auto computeShader = ComputeShaderCom::Create(
        _device,
        _context,
        shaderPath.c_str(),
        "CS_Main"
    );
    CHECK_NULL(computeShader, E_FAIL);

    // 4. HLSL register(t0/u0)와 C++ slot 0을 맞춘 뒤 Dispatch한다.
    CHECK_FAILED(computeShader->Bind_SRV(0, inputBuffer->Get_SRV()), E_FAIL);
    CHECK_FAILED(computeShader->Bind_UAV(0, outputBuffer->Get_UAV()), E_FAIL);

    const uint32 groupCount = (computeSmokeElementCount + computeSmokeThreadCount - 1) / computeSmokeThreadCount;
    CHECK_FAILED(computeShader->Dispatch(groupCount, 1, 1), E_FAIL);

    // 5. GPU output buffer는 CPU가 직접 읽을 수 없으므로 staging buffer로 복사한다.
    D3D11_BUFFER_DESC stagingDesc{};
    outputBuffer->Get_Buffer()->GetDesc(&stagingDesc);
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ComPtr<ID3D11Buffer> stagingBuffer{};
    CHECK_FAILED(_device->CreateBuffer(&stagingDesc, nullptr, stagingBuffer.GetAddressOf()), E_FAIL);

    _context->CopyResource(stagingBuffer.Get(), outputBuffer->Get_Buffer());

    // 6. staging buffer를 Map해서 CPU에서 결과값을 확인한다.
    D3D11_MAPPED_SUBRESOURCE mappedSubResource{};
    CHECK_FAILED(_context->Map(stagingBuffer.Get(), 0, D3D11_MAP_READ, 0, &mappedSubResource), E_FAIL);

    const Vec4* resultData = static_cast<const Vec4*>(mappedSubResource.pData);
    bool valid = true;
    for (uint32 i = 0; i < computeSmokeElementCount; ++i)
    {
        const Vec4 expected = inputData[i] * 2.f;
        if (resultData[i].x != expected.x || resultData[i].y != expected.y || resultData[i].z != expected.z || resultData[i].w != expected.w)
        {
            valid = false;
            break;
        }
    }

    _context->Unmap(stagingBuffer.Get(), 0);

    if (!valid)
    {
        LOG_ERROR("Compute shader smoke test failed: readback value mismatch.");
        return E_FAIL;
    }

    LOG_INFO("Compute shader smoke test passed.");
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

wstring EffectEditorApp::Get_ClientShaderPath(const wchar_t* fileName) const
{
    wchar_t modulePath[MAX_PATH] = {};
    if (0 == GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
        return {};

    // 런타임 위치로부터 ShaderFiles 폴더 찾아가는 코드
    const fs::path runtimeRoot = fs::path(modulePath).parent_path();
    const fs::path shaderRoot = fs::absolute(runtimeRoot / L"../../../Client/Bin/ShaderFiles").lexically_normal();

    if (nullptr == fileName || L'\0' == *fileName)
        return shaderRoot.wstring();

    return (shaderRoot / fileName).lexically_normal().wstring();
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
