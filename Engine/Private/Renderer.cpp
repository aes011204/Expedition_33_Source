#include "Renderer.h"

#include "GameInstance.h"
#include "GameObject.h"
#include "ShaderCom.h"
#include "VIBuffer_Rect.h"

Renderer::Renderer(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : _device{ device }
    , _context{ context }
{
}

HRESULT Renderer::Initialize()
{
    CHECK_FAILED(Ready_RenderTargets(), E_FAIL);

	return S_OK;
}

void Renderer::Add_RenderGroup(RenderGroup renderGroup, const Shared<GameObject>& gameObject)
{
    CHECK_NULL(gameObject);

    if (renderGroup < RenderGroup::Priority || renderGroup >= RenderGroup::END)
        return;

    _renderGroups[ETOI(renderGroup)].push_back(gameObject);
}

void Renderer::Draw()
{
    Change_ViewportDesc(GAME->Get_ViewportWidth(), GAME->Get_ViewportHeight());

#ifdef _DEBUG
    Reset_RenderCall();

    const auto renderBegin = chrono::steady_clock::now();
    double priorityMs = 0.0;
    double shadowMs = 0.0;
    double nonBlendMs = 0.0;
    double lightsMs = 0.0;
    double combinedMs = 0.0;
    double nonlightMs = 0.0;
    double blendMs = 0.0;
    double uiMs = 0.0;
    double debugMs = 0.0;

    const auto priorityBegin = chrono::steady_clock::now();
#endif

    Render_Priority();
#ifdef _DEBUG
    priorityMs = chrono::duration<double, milli>(chrono::steady_clock::now() - priorityBegin).count();
    const auto shadowBegin = chrono::steady_clock::now();
#endif

//    Render_Shadow();
#ifdef _DEBUG
    shadowMs = chrono::duration<double, milli>(chrono::steady_clock::now() - shadowBegin).count();
    const auto nonBlendBegin = chrono::steady_clock::now();
#endif

    Render_NonBlend();
#ifdef _DEBUG
    nonBlendMs = chrono::duration<double, milli>(chrono::steady_clock::now() - nonBlendBegin).count();
    const auto lightsBegin = chrono::steady_clock::now();
#endif

    Render_Lights();
#ifdef _DEBUG
    lightsMs = chrono::duration<double, milli>(chrono::steady_clock::now() - lightsBegin).count();
    const auto combinedBegin = chrono::steady_clock::now();
#endif
    
    Render_Combined();
#ifdef _DEBUG
    combinedMs = chrono::duration<double, milli>(chrono::steady_clock::now() - combinedBegin).count();
    const auto nonlightBegin = chrono::steady_clock::now();
#endif

    Render_NonLight();
#ifdef _DEBUG
    nonlightMs = chrono::duration<double, milli>(chrono::steady_clock::now() - nonlightBegin).count();
    const auto blendBegin = chrono::steady_clock::now();
#endif

    Render_Blend();
#ifdef _DEBUG
    blendMs = chrono::duration<double, milli>(chrono::steady_clock::now() - blendBegin).count();
    const auto uiBegin = chrono::steady_clock::now();
#endif

    Render_UI();
#ifdef _DEBUG
    uiMs = chrono::duration<double, milli>(chrono::steady_clock::now() - uiBegin).count();
    const auto debugBegin = chrono::steady_clock::now();
#endif

#ifdef _DEBUG
    Render_Debug();

    debugMs = chrono::duration<double, milli>(chrono::steady_clock::now() - debugBegin).count();
    const auto renderMs = chrono::duration<double, milli>(chrono::steady_clock::now() - renderBegin).count();
    Accumulate_RenderPerf(renderMs, priorityMs, shadowMs, nonBlendMs, lightsMs, combinedMs, nonlightMs, blendMs, uiMs, debugMs);
#endif
}

void Renderer::Render_Priority()
{
    for (auto& renderObject : _renderGroups[ETOI(RenderGroup::Priority)])
    {
        Shared<GameObject> gameObject = renderObject.lock();
        if (nullptr != gameObject)
        {
            gameObject->Render();
            Add_RenderCall();
        }
    }

    _renderGroups[ETOI(RenderGroup::Priority)].clear();
}

void Renderer::Render_Shadow()
{
    if (FAILED(GAME->Begin_MRT(L"MRT_ShadowObjects", true)))
        return;

    const uint32 originWidth = GAME->Get_ViewportWidth();
    const uint32 originHeight = GAME->Get_ViewportHeight();
    const uint32 shadowWidth = static_cast<uint32>(originWidth * _shadowScale);
    const uint32 shadowHeight = static_cast<uint32>(originHeight * _shadowScale);

    // 그림자용 사이즈로 변경
    Change_ViewportDesc(shadowWidth, shadowHeight);

    for (auto& renderObject : _renderGroups[ETOI(RenderGroup::Shadow)])
    {
        Shared<GameObject> gameObject = renderObject.lock();
        if (nullptr != gameObject)
        {
            gameObject->Render_Shadow();
            Add_RenderCall();
        }
    }

    _renderGroups[ETOI(RenderGroup::Shadow)].clear();

    CHECK_FAILED(GAME->End_MRT());

    Change_ViewportDesc(originWidth, originHeight);
}

void Renderer::Render_NonBlend()
{
    // Diffuse + Normal
    CHECK_FAILED(GAME->Begin_MRT(L"MRT_GameObjects", true));

    // TODO 필요 시 정렬
	for (auto& renderObject : _renderGroups[ETOI(RenderGroup::NonBlend)])
	{
		Shared<GameObject> gameObject = renderObject.lock();
		if (nullptr != gameObject)
        {
			gameObject->Render();
            Add_RenderCall();
        }
	}

	_renderGroups[ETOI(RenderGroup::NonBlend)].clear();

    CHECK_FAILED(GAME->End_MRT());
}


void Renderer::Render_Lights()
{
    if (!_deferredShader || !_viBuffer)
        return;

    if (FAILED(GAME->Begin_MRT(L"MRT_LightAcc")))
        return;

    // Transform
    ENDMRT_RETURN_IF_FAILED(
        _deferredShader->Bind_Matrix("g_WorldMatrix", &_deferredWorldMatrix));
    ENDMRT_RETURN_IF_FAILED(
        _deferredShader->Bind_Matrix("g_ViewMatrix", &_deferredViewMatrix));
    ENDMRT_RETURN_IF_FAILED(
        _deferredShader->Bind_Matrix("g_ProjMatrix", &_deferredProjMatrix));

    // depth -> World 위치 복원
    ENDMRT_RETURN_IF_FAILED(
        GAME->Bind_TransformMatrix_Inverse(D3DTS::View, _deferredShader.get(), "g_ViewMatrixInverse"));
    ENDMRT_RETURN_IF_FAILED(
        GAME->Bind_TransformMatrix_Inverse(D3DTS::Proj, _deferredShader.get(), "g_ProjMatrixInverse"));
    ENDMRT_RETURN_IF_FAILED(
        GAME->Bind_CamPosition(_deferredShader.get(), "g_vCamPosition"));
    ENDMRT_RETURN_IF_FAILED(
        GAME->Bind_CameraFar(_deferredShader, "g_fFar"));

    // 명암 연산을 위한 SRV
    ENDMRT_RETURN_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_deferredShader, "g_NormalTexture", L"Target_Normal"));
    ENDMRT_RETURN_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_deferredShader, "g_DepthTexture", L"Target_Depth"));

    // Light 기록
    ENDMRT_RETURN_IF_FAILED(
        _viBuffer->Bind_Resources());
    ENDMRT_RETURN_IF_FAILED(
        GAME->Render_Lights(_deferredShader, _viBuffer));
    Add_RenderCall();

    if (FAILED(GAME->End_MRT()))
        return;
}

void Renderer::Render_NonLight()
{
    for (auto& renderObject : _renderGroups[ETOI(RenderGroup::NonLight)])
    {
        Shared<GameObject> gameObject = renderObject.lock();
        if (nullptr != gameObject)
        {
            gameObject->Render();
            Add_RenderCall();
        }
    }

    _renderGroups[ETOI(RenderGroup::NonLight)].clear();
}

void Renderer::Render_Combined()
{
    if (!_deferredShader || !_viBuffer)
        return;

    // Transform
    if (FAILED(_deferredShader->Bind_Matrix("g_WorldMatrix", &_deferredWorldMatrix)))
        return;
    if (FAILED(_deferredShader->Bind_Matrix("g_ViewMatrix", &_deferredViewMatrix)))
        return;
    if (FAILED(_deferredShader->Bind_Matrix("g_ProjMatrix", &_deferredProjMatrix)))
        return;

    // depth -> World 위치 복원 용도
    if (FAILED(GAME->Bind_TransformMatrix_Inverse(D3DTS::View, _deferredShader.get(), "g_ViewMatrixInverse")))
        return;
    if (FAILED(GAME->Bind_TransformMatrix_Inverse(D3DTS::Proj, _deferredShader.get(), "g_ProjMatrixInverse")))
        return;
    if (FAILED(GAME->Bind_CamPosition(_deferredShader.get(), "g_vCamPosition")))
        return;
    if (FAILED(GAME->Bind_CameraFar(_deferredShader, "g_fFar")))
        return;

    // 그림자
    if (FAILED(GAME->Bind_ShadowMatrices(_deferredShader,  "g_LightViewMatrix", "g_LightProjMatrix")))
        return;
    if (FAILED(GAME->Bind_ShadowLightDesc(_deferredShader, "g_fLightFar", "g_fBias", "g_fShadowIntensity")))
        return;

    // SRV 바인딩
    if (FAILED(GAME->Bind_RT_ShaderResource(_deferredShader, "g_DiffuseTexture", L"Target_Diffuse")))
        return;
    if (FAILED(GAME->Bind_RT_ShaderResource(_deferredShader, "g_ShadeTexture", L"Target_Shade")))
        return;
    if (FAILED(GAME->Bind_RT_ShaderResource(_deferredShader, "g_SpecularTexture", L"Target_Specular")))
        return;
    if (FAILED(GAME->Bind_RT_ShaderResource(_deferredShader, "g_DepthTexture", L"Target_Depth")))
        return;
    if (FAILED(GAME->Bind_RT_ShaderResource(_deferredShader, "g_LightDepthTexture", L"Target_LightDepth")))
        return;

    // Light 기록
    if (FAILED(_viBuffer->Bind_Resources()))
        return;
    if (FAILED(_deferredShader->Begin(ETOI(DeferredPass::Combined))))
        return;
    if (FAILED(_viBuffer->Render()))
        return;
    Add_RenderCall();
}


void Renderer::Render_Blend()
{
    for (auto& renderObject : _renderGroups[ETOI(RenderGroup::Blend)])
    {
        Shared<GameObject> gameObject = renderObject.lock();
        if (nullptr != gameObject)
        {
            gameObject->Render();
            Add_RenderCall();
        }
    }

    _renderGroups[ETOI(RenderGroup::Blend)].clear();
}

void Renderer::Render_UI()
{
    for (auto& renderObject : _renderGroups[ETOI(RenderGroup::UI)])
    {
        Shared<GameObject> gameObject = renderObject.lock();
        if (nullptr != gameObject)
        {
            gameObject->Render();
            Add_RenderCall();
        }
    }

    _renderGroups[ETOI(RenderGroup::UI)].clear();
}

HRESULT Renderer::Ready_RenderTargets()
{
    const uint32 width = GAME->Get_ViewportWidth();
    const uint32 height = GAME->Get_ViewportHeight();

    const bool resizeWithViewport = true;
    const bool useDSV = true;

    // MRT_GameObjects
    CHECK_FAILED(GAME->Add_RenderTarget(L"Target_Diffuse",
        width, height, DXGI_FORMAT_R8G8B8A8_UNORM, Color(0, 0, 0, 0), resizeWithViewport, !useDSV), E_FAIL);

    CHECK_FAILED(GAME->Add_RenderTarget(L"Target_Normal",
        width, height, DXGI_FORMAT_R16G16B16A16_UNORM, Color(0, 0, 0, 0), resizeWithViewport, !useDSV), E_FAIL);

    CHECK_FAILED(GAME->Add_RenderTarget(L"Target_Depth",
        width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, Color(0, 0, 0, 0), resizeWithViewport, useDSV), E_FAIL);

    // MRT_LightAcc
    CHECK_FAILED(GAME->Add_RenderTarget(L"Target_Shade",
        width, height, DXGI_FORMAT_R16G16B16A16_UNORM, Color(0, 0, 0, 0), resizeWithViewport, !useDSV), E_FAIL);

    CHECK_FAILED(GAME->Add_RenderTarget(L"Target_Specular",
        width, height, DXGI_FORMAT_R16G16B16A16_UNORM, Color(0, 0, 0, 0), resizeWithViewport, !useDSV), E_FAIL);

    // MRT_ShadowObjects
    CHECK_FAILED(GAME->Add_RenderTarget(L"Target_LightDepth",
        width * static_cast<uint32>(_shadowScale), height * static_cast<uint32>(_shadowScale),
        DXGI_FORMAT_R32G32B32A32_FLOAT, Color(1.f, 1.f, 1.f, 1.f), !resizeWithViewport, useDSV), E_FAIL);

    //CHECK_FAILED(Ready_ShadowDSV(), E_FAIL);

    // Create MRT
    CHECK_FAILED(GAME->Add_MRT(L"MRT_GameObjects", L"Target_Diffuse"), E_FAIL); // SV_TARGET0
    CHECK_FAILED(GAME->Add_MRT(L"MRT_GameObjects", L"Target_Normal"), E_FAIL);  // SV_TARGET1
    CHECK_FAILED(GAME->Add_MRT(L"MRT_GameObjects", L"Target_Depth"), E_FAIL);   // SV_TARGET2

    CHECK_FAILED(GAME->Add_MRT(L"MRT_LightAcc", L"Target_Shade"), E_FAIL);      // SV_TARGET0
    CHECK_FAILED(GAME->Add_MRT(L"MRT_LightAcc", L"Target_Specular"), E_FAIL);   // SV_TARGET1

    CHECK_FAILED(GAME->Add_MRT(L"MRT_ShadowObjects", L"Target_LightDepth"), E_FAIL);

    // deferred shader, rect buffer 생성
    _viBuffer = VIBuffer_Rect::Create(_device, _context);
    CHECK_NULL(_viBuffer, E_FAIL);

    _deferredShader = ShaderCom::Create(_device, _context,
        L"../../../Client/Bin/ShaderFiles/Shader_Deferred.hlsl", VTXTEX::Elements, VTXTEX::numElements, ShaderType::VtxTex);
    CHECK_NULL(_deferredShader, E_FAIL);

    Sync_ViewportDependentState(width, height);

#ifdef _DEBUG
    // 전체 뷰포트의 1/4만 사용하여 디버그 렌더
    GAME->Initialize_Debug(width * 0.5f, height * 0.5f);
#endif

    return S_OK;
}

void Renderer::Sync_ViewportDependentState(uint32 width, uint32 height)
{
    width = max(1u, width);
    height = max(1u, height);

    _deferredWorldMatrix = Matrix::CreateScale(static_cast<float>(width), static_cast<float>(height), 1.f);
    _deferredViewMatrix = Matrix::Identity;
    _deferredProjMatrix = XMMatrixOrthographicLH(static_cast<float>(width), static_cast<float>(height), 0.f, 1.f);

    Change_ViewportDesc(width, height);
}

void Renderer::Change_ViewportDesc(uint32 width, uint32 height)
{
    D3D11_VIEWPORT  ViewportDesc{};
    ZeroMemory(&ViewportDesc, sizeof(ViewportDesc));

    ViewportDesc.TopLeftX = 0;
    ViewportDesc.TopLeftY = 0;
    ViewportDesc.Width = static_cast<float>(width);
    ViewportDesc.Height = static_cast<float>(height);
    ViewportDesc.MinDepth = 0.f;
    ViewportDesc.MaxDepth = 1.f;

    _context->RSSetViewports(1, &ViewportDesc);
}

#ifdef _DEBUG

uint32 Renderer::Get_RenderCall() const
{
    return iDrawCall;
}

void Renderer::Reset_RenderCall()
{
    iDrawCall = 0;
}

void Renderer::Add_RenderCall(uint32 count)
{
    iDrawCall += count;
}

bool Renderer::Try_ConsumePerfSummary(const float windowSec, RenderPerfSummary& outSummary)
{
    outSummary = {};

    if (windowSec <= 0.f || 0 == _renderPerfWindow.render.sampleCount)
        return false;

    outSummary.valid = true;
    outSummary.renderCallCount = static_cast<uint32>(_renderPerfWindow.render.sampleCount);

    outSummary.renderAvgMs = static_cast<float>(
        Safe_Divide(_renderPerfWindow.render.accumulatedMs, static_cast<double>(_renderPerfWindow.render.sampleCount)));
    outSummary.renderMaxMs = static_cast<float>(_renderPerfWindow.render.maxMs);

    outSummary.priorityAvgMs = static_cast<float>(
        Safe_Divide(_renderPerfWindow.priority.accumulatedMs, static_cast<double>(_renderPerfWindow.priority.sampleCount)));
    outSummary.priorityMaxMs = static_cast<float>(_renderPerfWindow.priority.maxMs);

    outSummary.shadowAvgMs = static_cast<float>(
        Safe_Divide(_renderPerfWindow.shadow.accumulatedMs, static_cast<double>(_renderPerfWindow.shadow.sampleCount)));
    outSummary.shadowMaxMs = static_cast<float>(_renderPerfWindow.shadow.maxMs);

    outSummary.nonBlendAvgMs = static_cast<float>(
        Safe_Divide(_renderPerfWindow.nonBlend.accumulatedMs, static_cast<double>(_renderPerfWindow.nonBlend.sampleCount)));
    outSummary.nonBlendMaxMs = static_cast<float>(_renderPerfWindow.nonBlend.maxMs);

    outSummary.lightsAvgMs = static_cast<float>(
        Safe_Divide(_renderPerfWindow.lights.accumulatedMs, static_cast<double>(_renderPerfWindow.lights.sampleCount)));
    outSummary.lightsMaxMs = static_cast<float>(_renderPerfWindow.lights.maxMs);

    outSummary.combinedAvgMs = static_cast<float>(
        Safe_Divide(_renderPerfWindow.combined.accumulatedMs, static_cast<double>(_renderPerfWindow.combined.sampleCount)));
    outSummary.combinedMaxMs = static_cast<float>(_renderPerfWindow.combined.maxMs);

    outSummary.nonLightAvgMs = static_cast<float>(
        Safe_Divide(_renderPerfWindow.nonLight.accumulatedMs, static_cast<double>(_renderPerfWindow.nonLight.sampleCount)));
    outSummary.nonLightMaxMs = static_cast<float>(_renderPerfWindow.nonLight.maxMs);

    outSummary.blendAvgMs = static_cast<float>(
        Safe_Divide(_renderPerfWindow.blend.accumulatedMs, static_cast<double>(_renderPerfWindow.blend.sampleCount)));
    outSummary.blendMaxMs = static_cast<float>(_renderPerfWindow.blend.maxMs);

    outSummary.uiAvgMs = static_cast<float>(
        Safe_Divide(_renderPerfWindow.ui.accumulatedMs, static_cast<double>(_renderPerfWindow.ui.sampleCount)));
    outSummary.uiMaxMs = static_cast<float>(_renderPerfWindow.ui.maxMs);

    outSummary.debugAvgMs = static_cast<float>(
        Safe_Divide(_renderPerfWindow.debug.accumulatedMs, static_cast<double>(_renderPerfWindow.debug.sampleCount)));
    outSummary.debugMaxMs = static_cast<float>(_renderPerfWindow.debug.maxMs);

    Reset_RenderPerfWindow(_renderPerfWindow);
    return true;
}

void Renderer::Accumulate_RenderPerf(
    const double renderMs,
    const double priorityMs,
    const double shadowMs,
    const double nonBlendMs,
    const double lightsMs,
    const double combinedMs,
    const double nonLightMs,
    const double blendMs,
    const double uiMs,
    const double debugMs)
{
    Accumulate_PassPerf(_renderPerfWindow.render, renderMs);
    Accumulate_PassPerf(_renderPerfWindow.priority, priorityMs);
    Accumulate_PassPerf(_renderPerfWindow.shadow, shadowMs);
    Accumulate_PassPerf(_renderPerfWindow.nonBlend, nonBlendMs);
    Accumulate_PassPerf(_renderPerfWindow.lights, lightsMs);
    Accumulate_PassPerf(_renderPerfWindow.combined, combinedMs);
    Accumulate_PassPerf(_renderPerfWindow.nonLight, nonLightMs);
    Accumulate_PassPerf(_renderPerfWindow.blend, blendMs);
    Accumulate_PassPerf(_renderPerfWindow.ui, uiMs);
    Accumulate_PassPerf(_renderPerfWindow.debug, debugMs);
}

void Renderer::Accumulate_PassPerf(PerfCounter& counter, const double passMs)
{
    ++counter.sampleCount;
    counter.accumulatedMs += passMs;
    counter.maxMs = max(counter.maxMs, passMs);
}

void Renderer::Reset_RenderPerfWindow(RenderPerfWindow& window)
{
    window = {};
}

double Renderer::Safe_Divide(const double numerator, const double denominator)
{
    if (denominator <= 0.0)
        return 0.0;

    return numerator / denominator;
}
#endif

Unique<Renderer> Renderer::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    Unique<Renderer> instance{ new Renderer(device, context) };

    if (FAILED(instance->Initialize()))
    {
        MSG_BOX("Failed to Created : Renderer");
        instance->Free();
        return nullptr;
    }
    return instance;
}

void Renderer::Free()
{
    __super::Free();

    for (size_t i = 0; i < ETOI(RenderGroup::END); i++)
        _renderGroups[i].clear();
}

#ifdef _DEBUG

void Renderer::Add_DebugRenderGroup(Shared<Component> debugComponent)
{
    _debugComponents.push_back(debugComponent);
}

void Renderer::Render_Debug()
{
    // TODO : Component 에 해당 함수 추가 후 작성
    //for (auto& debugCom : _debugComponents)
    //    debugCom->Render_Debug();

    _debugComponents.clear();

    // 디버그 RT 프리뷰는 Deferred 셰이더의 VS_MAIN(= g_ViewMatrix/g_ProjMatrix 사용)로 그린다.
    // 다른 패스/리사이즈로 값이 바뀌면 화면 밖으로 날아갈 수 있으니 여기서 매번 명시적으로 바인딩한다.
    if (_deferredShader)
    {
        const uint32 width = GAME->Get_ViewportWidth();
        const uint32 height = GAME->Get_ViewportHeight();

        _deferredViewMatrix = Matrix::Identity;
        _deferredProjMatrix = XMMatrixOrthographicLH(static_cast<float>(width), static_cast<float>(height), 0.f, 1.f);

        _deferredShader->Bind_Matrix("g_ViewMatrix", &_deferredViewMatrix);
        _deferredShader->Bind_Matrix("g_ProjMatrix", &_deferredProjMatrix);
    }

    CHECK_FAILED(GAME->Render_RT_Debug(_viBuffer, _deferredShader));
    Add_RenderCall();
}

#endif

#ifdef ENDMRT_RETURN_IF_FAILED
#undef ENDMRT_RETURN_IF_FAILED
#endif
