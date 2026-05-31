#include "Renderer.h"
#include "Camera.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "LightingPipeline.h"
#include "PostProcessPipeline.h"
#include "ScreenFx_Manager.h"
#include "ShaderCom.h"
#include "Texture.h"
#include "VIBuffer_Rect.h"
#include "Debug_Profile.h"

#pragma region DEBUG
#ifdef _DEBUG
namespace
{
    wstring PtrToWString(const void* ptr)
    {
        wchar_t buffer[32] = {};
        swprintf_s(buffer, L"0x%p", ptr);
        return buffer;
    }

    bool Try_GetObjectName(ID3D11DeviceChild* child, wstring& outName)
    {
        if (nullptr == child)
            return false;

        wchar_t nameW[256] = {};
        UINT sizeW = sizeof(nameW);
        if (SUCCEEDED(child->GetPrivateData(WKPDID_D3DDebugObjectNameW, &sizeW, nameW)) &&
            sizeW > sizeof(wchar_t) && nameW[0] != L'\0')
        {
            outName = nameW;
            return true;
        }

        char nameA[256] = {};
        UINT sizeA = sizeof(nameA);
        if (SUCCEEDED(child->GetPrivateData(WKPDID_D3DDebugObjectName, &sizeA, nameA)) &&
            sizeA > 0 && nameA[0] != '\0')
        {
            int length = MultiByteToWideChar(CP_UTF8, 0, nameA, -1, nullptr, 0);
            if (length <= 0)
                length = MultiByteToWideChar(CP_ACP, 0, nameA, -1, nullptr, 0);

            if (length > 1)
            {
                vector<wchar_t> wideBuffer(static_cast<size_t>(length), L'\0');
                if (MultiByteToWideChar(CP_UTF8, 0, nameA, -1, wideBuffer.data(), length) <= 0)
                {
                    MultiByteToWideChar(CP_ACP, 0, nameA, -1, wideBuffer.data(), length);
                }
                outName.assign(wideBuffer.data());
                return true;
            }
        }

        return false;
    }
}
#endif
#pragma endregion

Renderer::Renderer(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : _device{ device }
    , _context{ context }
{
}

HRESULT Renderer::Initialize()
{
    CHECK_FAILED(GAME->Ready_AllRenderTargets(), E_FAIL);               // 렌더타겟 모두 여기서 준비
    CHECK_FAILED(Ready_Shaders(), E_FAIL);

    _ppPipeline = PostProcessPipeline::Create(_device, _context);
    CHECK_NULL(_ppPipeline, E_FAIL);

    _lightingPipeline = LightingPipeline::Create(_device, _context);
    CHECK_NULL(_lightingPipeline, E_FAIL);

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
    // ======================================================================================================
    // 렌더 패스 추가하고 싶은데 _DEBUG 이런 거 때문에 넘 복잡하다 싶으면 제게 말해주세요~ ( 코덱스가 더 잘하긴 함 )
    // ======================================================================================================

    _screenSize = { GAME->Get_ViewportWidth(), GAME->Get_ViewportHeight() };

    Change_ViewportDesc(_screenSize.width, _screenSize.height);

#ifdef _DEBUG
    DumpCurrentOMState(L"Renderer::Draw:Begin");
    Reset_RenderCall();

#pragma region DEBUG_VAR
    const auto renderBegin = chrono::steady_clock::now();
    double priorityMs = 0.0;
    double shadowMs = 0.0;
    double nonBlendMs = 0.0;
    double lightsMs = 0.0;
    double combinedHDRMs = 0.0;
    double postProcessMs = 0.0;
    double nonlightMs = 0.0;
    double blendMs = 0.0;
    double uiMs = 0.0;
    double debugMs = 0.0;

    const auto priorityBegin = chrono::steady_clock::now();
#pragma endregion

    DumpCurrentOMState(L"Render_Priority:Before");
#endif
    // ========================================= //
    Render_Priority();
    // ========================================= //
#ifdef _DEBUG
    DumpCurrentOMState(L"Render_Priority:After");
    priorityMs = chrono::duration<double, milli>(chrono::steady_clock::now() - priorityBegin).count();
    const auto shadowBegin = chrono::steady_clock::now();
    DumpCurrentOMState(L"Render_Shadow:Before");
#endif

    // ========================================= //
     Render_Shadow();
    // ========================================= //
#ifdef _DEBUG
    shadowMs = chrono::duration<double, milli>(chrono::steady_clock::now() - shadowBegin).count();
    const auto nonBlendBegin = chrono::steady_clock::now();
    DumpCurrentOMState(L"Render_NonBlend:Before");
#endif

    // ========================================= //
    Render_NonBlend();
    // ========================================= //
#ifdef _DEBUG
    DumpCurrentOMState(L"Render_NonBlend:After");
    nonBlendMs = chrono::duration<double, milli>(chrono::steady_clock::now() - nonBlendBegin).count();
    DumpCurrentOMState(L"Render_SSAO:Before");
#endif

    // ========================================= //
    Render_SSAO();
    // ========================================= //
#ifdef _DEBUG
    DumpCurrentOMState(L"Render_SSAO:After");
    const auto lightsBegin = chrono::steady_clock::now();
    DumpCurrentOMState(L"Render_Lights:Before");
#endif

    // ========================================= //
    Render_Lights();
    // ========================================= //
#ifdef _DEBUG
    DumpCurrentOMState(L"Render_Lights:After");
    lightsMs = chrono::duration<double, milli>(chrono::steady_clock::now() - lightsBegin).count();
    const auto combinedHDRBegin = chrono::steady_clock::now();
    DumpCurrentOMState(L"Render_CombinedHDR:Before");
#endif

    Render_CombinedHDR();
#ifdef _DEBUG
    DumpCurrentOMState(L"Render_CombinedHDR:After");
    combinedHDRMs = chrono::duration<double, milli>(chrono::steady_clock::now() - combinedHDRBegin).count();
    const auto nonlightBegin = chrono::steady_clock::now();
    DumpCurrentOMState(L"Render_NonLight:Before");
#endif

    // ========================================= //
    Render_NonLight();
    // ========================================= //
#ifdef _DEBUG
    DumpCurrentOMState(L"Render_NonLight:After");
    nonlightMs = chrono::duration<double, milli>(chrono::steady_clock::now() - nonlightBegin).count();
    DumpCurrentOMState(L"Render_Distortion:Before");
#endif

    // ========================================= //
    Render_Distortion();
    // ========================================= //
#ifdef _DEBUG
    DumpCurrentOMState(L"Render_Distortion:After");
    const auto blendBegin = chrono::steady_clock::now();
    DumpCurrentOMState(L"Render_Blend:Before");
#endif

    // ========================================= //
    Render_Blend();
    // ========================================= //
#ifdef _DEBUG
    DumpCurrentOMState(L"Render_Blend:After");
    blendMs = chrono::duration<double, milli>(chrono::steady_clock::now() - blendBegin).count();
    const auto postProcesBegin = chrono::steady_clock::now();
    DumpCurrentOMState(L"Render_PostProcess:Before");
#endif

    // ========================================= //
    Render_PostProcess();
    // ========================================= //
#ifdef _DEBUG
    DumpCurrentOMState(L"Render_PostProcess:After");
    postProcessMs = chrono::duration<double, milli>(chrono::steady_clock::now() - postProcesBegin).count();
    const auto uiBegin = chrono::steady_clock::now();
    DumpCurrentOMState(L"Render_ScreenFX:Before");
#endif

    //GAME->Bind_BackBuffer();

    const ScreenFx_Manager* screenFxManager = GAME->Get_ScreenFxManager();
    const bool useScreenFx = screenFxManager && screenFxManager->Is_ScreenFXActivated() && screenFxManager->Has_ActiveEffects() && _screenFxShader;
    if (useScreenFx)
    {
        // ========================================= //
        Render_ScreenFx();
        // ========================================= //
    }

#ifdef _DEBUG
    DumpCurrentOMState(L"Render_ScreenFX:After");
    uiMs = chrono::duration<double, milli>(chrono::steady_clock::now() - uiBegin).count();
    const auto debugBegin = chrono::steady_clock::now();
#endif

#ifdef _DEBUG
    Render_Debug();

    debugMs = chrono::duration<double, milli>(chrono::steady_clock::now() - debugBegin).count();
    const auto renderMs = chrono::duration<double, milli>(chrono::steady_clock::now() - renderBegin).count();
    Accumulate_RenderPerf(renderMs, priorityMs, shadowMs, nonBlendMs, lightsMs, combinedHDRMs, postProcessMs,nonlightMs, blendMs, uiMs, debugMs);
#endif
}

void Renderer::Backup_RenderGroups()
{
    for (size_t i = 0; i < ETOI(RenderGroup::END); ++i)
    {
        _renderGroupBackup[i] = _renderGroups[i];
        _renderGroups[i].clear();
    }

    _hasRenderGroupBackup = true;
}

void Renderer::Restore_RenderGroups()
{
    if (!_hasRenderGroupBackup)
        return;

    for (size_t i = 0; i < ETOI(RenderGroup::END); ++i)
    {
        _renderGroups[i] = _renderGroupBackup[i];
        _renderGroupBackup[i].clear();
    }

    _hasRenderGroupBackup = false;
}

void Renderer::Render_Priority()
{
    ENDMRT_RETURN_IF_FAILED(GAME->Begin_MRT(MRT::HDR, NotUseDSV, ClearRTV));

    _currentGroup = RenderGroup::Priority;

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

    GAME->End_MRT();
}

void Renderer::Render_Shadow()
{
    _currentGroup = RenderGroup::Shadow;

    Render_StaticShadowOnce();
    Render_DynamicShadow();
}

void Renderer::Render_NonBlend()
{
    // Diffuse + Normal / NotUseDSV -> UseDSV
    CHECK_FAILED(GAME->Begin_MRT(MRT::GameObjects, NotUseDSV, ClearRTV));

    _currentGroup = RenderGroup::NonBlend;

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

void Renderer::Render_SSAO()
{
    if (!_deferredShader || !_viBuffer)
        return;

    // SSAO 사용 여부
    const bool ssaoEnabled = (_lightingPipeline != nullptr) && _lightingPipeline->Is_Enabled();
    if (!ssaoEnabled)
    {
        // SSAO를 끈 프레임에는 이전 AO 결과가 남지 않도록 RT만 기본값(1.0)으로 초기화한다.
        RETURN_LOG_IF_FAILED(
            GAME->Begin_MRT(MRT::SSAO, NotUseDSV, ClearRTV),
            L"Render_SSAO:FAIL Begin_MRT(SSAO-ClearOnly)");

        if (FAILED(GAME->End_MRT()))
            return;

        RETURN_LOG_IF_FAILED(
            GAME->Begin_MRT(MRT::SSAOBlur, NotUseDSV, ClearRTV),
            L"Render_SSAO:FAIL Begin_MRT(SSAOBlur-ClearOnly)");

        if (FAILED(GAME->End_MRT()))
            return;

        return;
    }

    // SSAO 패스 : AO 값 계산/추출하기
    {
        RETURN_LOG_IF_FAILED(
           GAME->Begin_MRT(MRT::SSAO, NotUseDSV, ClearRTV),
           L"Render_SSAO:FAIL Begin_MRT");

        ENDMRT_RETURN_IF_FAILED(
            _deferredShader->Bind_ObjectCB(_pipelineWorldMatrix));

        ENDMRT_RETURN_IF_FAILED(
            GAME->Bind_CameraCB(_deferredShader));

        ENDMRT_RETURN_IF_FAILED(
            Bind_ScreenSpaceCB(_deferredShader));

        ENDMRT_RETURN_IF_FAILED(
            GAME->Bind_RT_ShaderResource(_deferredShader, "g_NormalTexture", RT::Normal));

        ENDMRT_RETURN_IF_FAILED(
            GAME->Bind_RT_ShaderResource(_deferredShader, "g_DepthTexture", RT::Depth));

        SSAOCB ssaoParams{};
        if (_lightingPipeline)
            ssaoParams = _lightingPipeline->Get_Params();

        ENDMRT_RETURN_IF_FAILED(_deferredShader->Bind_CBufferData(ssaoParams));

        ENDMRT_RETURN_IF_FAILED(
            _viBuffer->Bind_Resources());

        ENDMRT_RETURN_IF_FAILED(
            _deferredShader->Begin(ETOI(DeferredPass::SSAO)));

        ENDMRT_RETURN_IF_FAILED(
            _viBuffer->Render());

        if (FAILED(GAME->End_MRT()))
            return;

        Add_RenderCall();
    }

    // SSAO Blur 패스 : AO 값을 경계 보존하며 부드럽게 만들기
    {
        if (_lightingPipeline == nullptr || _lightingPipeline->Get_Params().SSAOBilateralEnabled == 0u)
            return;

        // MRT 바뀌었으니 다시 바인딩
        RETURN_LOG_IF_FAILED(
            GAME->Begin_MRT(MRT::SSAOBlur, NotUseDSV, ClearRTV),
            L"Render_SSAO:FAIL Begin_MRT(SSAOBlur)");

        ENDMRT_RETURN_IF_FAILED(
            _deferredShader->Bind_ObjectCB(_pipelineWorldMatrix));

        ENDMRT_RETURN_IF_FAILED(
            Bind_ScreenSpaceCB(_deferredShader));

        ENDMRT_RETURN_IF_FAILED(
            GAME->Bind_RT_ShaderResource(_deferredShader, "g_SSAOTexture", RT::SSAO));

        ENDMRT_RETURN_IF_FAILED(
            GAME->Bind_RT_ShaderResource(_deferredShader, "g_NormalTexture", RT::Normal));

        ENDMRT_RETURN_IF_FAILED(
            GAME->Bind_RT_ShaderResource(_deferredShader, "g_DepthTexture", RT::Depth));

        ENDMRT_RETURN_IF_FAILED(
            _deferredShader->Bind_CBufferData(_lightingPipeline->Get_Params()));

        ENDMRT_RETURN_IF_FAILED(
            _viBuffer->Bind_Resources());

        ENDMRT_RETURN_IF_FAILED(
            _deferredShader->Begin(ETOI(DeferredPass::SSAOBlur)));

        ENDMRT_RETURN_IF_FAILED(
            _viBuffer->Render());

        if (FAILED(GAME->End_MRT()))
            return;
    }

    Add_RenderCall();
}

void Renderer::Render_Lights()
{
    if (!_deferredShader || !_viBuffer)
        return;

    if (FAILED(GAME->Begin_MRT(MRT::LightAcc, NotUseDSV, ClearRTV)))
        return;

    // Transform
    ENDMRT_RETURN_IF_FAILED(
        _deferredShader->Bind_ObjectCB(_pipelineWorldMatrix));

    // depth -> World 위치 복원
    ENDMRT_RETURN_IF_FAILED(Bind_ScreenSpaceCB(_deferredShader));

    // 명암 연산을 위한 SRV
    ENDMRT_RETURN_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_deferredShader, "g_DiffuseTexture", RT::Diffuse));
    ENDMRT_RETURN_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_deferredShader, "g_NormalTexture", RT::Normal));
    ENDMRT_RETURN_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_deferredShader, "g_DepthTexture", RT::Depth));
    ENDMRT_RETURN_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_deferredShader, "g_MaterialTexture", RT::Material));

    // Light 기록
    ENDMRT_RETURN_IF_FAILED(
        _viBuffer->Bind_Resources());
    ENDMRT_RETURN_IF_FAILED(
        GAME->Render_Lights(_deferredShader, _viBuffer));
    Add_RenderCall();

    if (FAILED(GAME->End_MRT()))
        return;
}

void Renderer::Render_CombinedHDR()
{
    if (!_deferredShader || !_viBuffer)
        return;

    bool postProcess = false;
    if (_ppPipeline && _ppPipeline->Has_PostProcessFeature(PostProcessFeature::HDR))
    {
        if (FAILED(GAME->Begin_MRT(MRT::HDR, NotUseDSV, NotClearRTV)))
            return;

        postProcess = true;
    }

    // Transform
    COMBINEDHDR_RETURN_LOG_IF_FAILED(
        _deferredShader->Bind_ObjectCB(_pipelineWorldMatrix),
        L"Render_CombinedHDR:FAIL Bind_ObjectCB");

    // depth -> World 위치 복원 용도
    COMBINEDHDR_RETURN_LOG_IF_FAILED(
        Bind_ScreenSpaceCB(_deferredShader),
        L"Render_CombinedHDR:FAIL Bind_ScreenSpaceCB");

    // 그림자
    COMBINEDHDR_RETURN_LOG_IF_FAILED(
        GAME->Bind_ShadowCB(_deferredShader),
        L"Render_CombinedHDR:FAIL Bind_ShadowCB");

    // SRV 바인딩
    COMBINEDHDR_RETURN_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_deferredShader, "g_DiffuseTexture", RT::Diffuse),
        L"Render_CombinedHDR:FAIL Bind_RT_ShaderResource(g_DiffuseTexture)");

    COMBINEDHDR_RETURN_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_deferredShader, "g_ShadeTexture", RT::Shade),
        L"Render_CombinedHDR:FAIL Bind_RT_ShaderResource(g_ShadeTexture)");

    COMBINEDHDR_RETURN_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_deferredShader, "g_AmbientTexture", RT::Ambient),
        L"Render_CombinedHDR:FAIL Bind_RT_ShaderResource(g_AmbientTexture)");

    COMBINEDHDR_RETURN_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_deferredShader, "g_SpecularTexture", RT::Specular),
        L"Render_CombinedHDR:FAIL Bind_RT_ShaderResource(g_SpecularTexture)");

    COMBINEDHDR_RETURN_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_deferredShader, "g_DepthTexture", RT::Depth),
        L"Render_CombinedHDR:FAIL Bind_RT_ShaderResource(g_DepthTexture)");

    COMBINEDHDR_RETURN_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_deferredShader, "g_LightDepthTexture", RT::LightDepth),
        L"Render_CombinedHDR:FAIL Bind_RT_ShaderResource(g_LightDepthTexture)");

    COMBINEDHDR_RETURN_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_deferredShader, "g_StaticLightDepthTexture", RT::StaticLightDepth),
        L"Render_CombinedHDR:FAIL Bind_RT_ShaderResource(g_StaticLightDepthTexture)");

    COMBINEDHDR_RETURN_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_deferredShader, "g_EmissiveTexture", RT::Emissive),
        L"Render_CombinedHDR:FAIL Bind_RT_ShaderRender_CombinedHDRResource(g_EmissiveTexture)");

    // Bilateral Blur : 거리, 깊이, 노말에 따라 Blur 가중치를 조절하는 기능
    const bool ssaoEnabled = (_lightingPipeline != nullptr) && _lightingPipeline->Is_Enabled();
    const wchar_t* ssaoResult = (ssaoEnabled && _lightingPipeline->Get_Params().SSAOBilateralEnabled != 0u)
        ? RT::SSAOBlur
        : RT::SSAO;

    COMBINEDHDR_RETURN_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_deferredShader, "g_SSAOTexture", ssaoResult),
        L"Render_CombinedHDR:FAIL Bind_RT_ShaderRender_CombinedHDRResource(g_SSAOTexture)");

    if (_iblIrradianceTexture != nullptr)
    {
        COMBINEDHDR_RETURN_LOG_IF_FAILED(
            _iblIrradianceTexture->Bind_ShaderResourceView(_deferredShader.get(), "g_IrradianceMap", 0),
            L"Render_CombinedHDR:FAIL Bind_ShaderResourceView(g_IrradianceMap)");
    }
    else
    {
        COMBINEDHDR_RETURN_LOG_IF_FAILED(
            _deferredShader->Bind_SRV("g_IrradianceMap", nullptr),
            L"Render_CombinedHDR:FAIL ClearSRV(g_IrradianceMap)");
    }

    if (_iblPrefilterTexture != nullptr)
    {
        COMBINEDHDR_RETURN_LOG_IF_FAILED(
            _iblPrefilterTexture->Bind_ShaderResourceView(_deferredShader.get(), "g_PrefilterMap", 0),
            L"Render_CombinedHDR:FAIL Bind_ShaderResourceView(g_PrefilterMap)");
    }
    else
    {
        COMBINEDHDR_RETURN_LOG_IF_FAILED(
            _deferredShader->Bind_SRV("g_PrefilterMap", nullptr),
            L"Render_CombinedHDR:FAIL ClearSRV(g_PrefilterMap)");
    }

    if (_iblBrdfLutTexture != nullptr)
    {
        COMBINEDHDR_RETURN_LOG_IF_FAILED(
            _iblBrdfLutTexture->Bind_ShaderResourceView(_deferredShader.get(), "g_BRDFLUT", 0),
            L"Render_CombinedHDR:FAIL Bind_ShaderResourceView(g_BRDFLUT)");
    }
    else
    {
        COMBINEDHDR_RETURN_LOG_IF_FAILED(
            _deferredShader->Bind_SRV("g_BRDFLUT", nullptr),
            L"Render_CombinedHDR:FAIL ClearSRV(g_BRDFLUT)");
    }

    DeferredLightingCB deferredLightingParams{};
    if (_lightingPipeline)
        deferredLightingParams = _lightingPipeline->Get_DeferredLightingParams();

    if (_iblDebugMode == IBLDebugMode::SpecularOnly)
    {
        deferredLightingParams.iblIntensity = 1.0f;
        deferredLightingParams.iblDiffuseIntensity = 0.0f;
        deferredLightingParams.iblSpecularIntensity = 1.0f;
    }
    else if (_iblDebugMode == IBLDebugMode::DiffuseOnly)
    {
        deferredLightingParams.iblIntensity = 1.0f;
        deferredLightingParams.iblDiffuseIntensity = 1.0f;
        deferredLightingParams.iblSpecularIntensity = 0.0f;
    }

    COMBINEDHDR_RETURN_LOG_IF_FAILED(
        _deferredShader->Bind_CBufferData(deferredLightingParams),
        L"Render_CombinedHDR:FAIL Bind_CBufferData(DeferredLightingCB)");

    // Light 기록
    COMBINEDHDR_RETURN_LOG_IF_FAILED(
        _viBuffer->Bind_Resources(),
        L"Render_CombinedHDR:FAIL Bind_Resources");

    COMBINEDHDR_RETURN_LOG_IF_FAILED(
        _deferredShader->Begin(ETOI(DeferredPass::CombinedHDR)),
        L"Render_CombinedHDR:FAIL ShaderBegin(CombinedHDR)");

    COMBINEDHDR_RETURN_LOG_IF_FAILED(
        _viBuffer->Render(),
        L"Render_CombinedHDR:FAIL RenderRect");

    if (postProcess)
    {
        COMBINEDHDR_RETURN_LOG_IF_FAILED(
            GAME->End_MRT(),
            L"Render_CombinedHDR:FAIL End_MRT(HDR)");
    }

    Add_RenderCall();
}
void Renderer::Render_NonLight()
{
    RETURN_LOG_IF_FAILED(
        GAME->Begin_MRT(MRT::HDR, NotUseDSV, NotClearRTV),
        L"Begin_MRT failed");

    _currentGroup = RenderGroup::NonLight;

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

    GAME->End_MRT();
}

void Renderer::Render_Distortion()
{
    auto& distortionGroup = _renderGroups[ETOI(RenderGroup::Distortion)];
    if (distortionGroup.empty())
        return;

    RETURN_LOG_IF_FAILED(
        GAME->Copy_RTV(RT::HDRColor, RT::DistortionSource),
        L"Render_Distortion:FAIL Copy_RTV(HDRColor -> DistortionSource)"
    );

    RETURN_LOG_IF_FAILED(
        GAME->Begin_MRT(MRT::DistortionField, NotUseDSV, ClearRTV),
        L"Render_Distortion:FAIL Begin_MRT(DistortionField)"
    );

    _currentGroup = RenderGroup::Distortion;

    for (auto& renderObject : distortionGroup)
    {
        Shared<GameObject> gameObject = renderObject.lock();
        if (nullptr != gameObject)
        {
            gameObject->Render();
            Add_RenderCall();
        }
    }

    distortionGroup.clear();

    GAME->End_MRT();

    RETURN_LOG_IF_FAILED(
        Render_DistortionResolve(),
        L"Render_Distortion:FAIL Render_DistortionResolve"
    );
}

void Renderer::Render_Blend()
{
    RETURN_LOG_IF_FAILED(
        GAME->Begin_MRT(MRT::HDR, NotUseDSV, NotClearRTV),
        L"Begin_MRT failed");

    _currentGroup = RenderGroup::Blend;

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

    GAME->End_MRT();
}

void Renderer::Render_PostProcess()
{
    if (!_postprocessShader || !_viBuffer)
        return;

    if (!_ppPipeline || _ppPipeline->Get_PostProcessFeatures() == PostProcessFeature::None)

        return;

    if (!_ppPipeline->Has_PostProcessFeature(PostProcessFeature::HDR))
        return;

    // Transform
    RETURN_LOG_IF_FAILED(
        _postprocessShader->Bind_ObjectCB(_pipelineWorldMatrix),
        L"Render_PostProcess:FAIL Bind_ObjectCB");

    // PostProcess Params
    auto& params = _ppPipeline->Get_PostProcessParams();
    const uint32 viewportWidth = max(1u, _screenSize.width);
    const uint32 viewportHeight = max(1u, _screenSize.height);
    RETURN_LOG_IF_FAILED(
        _postprocessShader->Bind_CBufferData(_ppPipeline->Get_PostProcessParams()),
        L"Render_PostProcess:FAIL Bind_CBufferData(Base)");

    // depth -> World 위치 복원 용도
    RETURN_LOG_IF_FAILED(
        Bind_ScreenSpaceCB(_postprocessShader, viewportWidth, viewportHeight),
        L"Render_PostProcess:FAIL Bind_ScreenSpaceCB");

    // IASet...
    RETURN_LOG_IF_FAILED(
        _viBuffer->Bind_Resources(),
        L"Render_PostProcess:FAIL Bind_Resources");

    // Bloom Mip Chain 구조. 해상도를 낮춰서 샘플링 횟수를 줄인다.
    const bool useBloom = _ppPipeline->Has_PostProcessFeature(PostProcessFeature::Bloom);
    if (useBloom)
    {
        const ViewportSize half = { max(1u, viewportWidth / 2u), max(1u, viewportHeight / 2u) };
        const ViewportSize quarter = { max(1u, viewportWidth / 2u), max(1u, viewportHeight / 2u) };
        const ViewportSize eighth = { max(1u, viewportWidth / 2u), max(1u, viewportHeight / 2u) };

        auto bindBloomScreenSpace = [&](uint32 srcWidth, uint32 srcHeight, const wchar_t* logTag)
        {
            RETURN_LOG_IF_FAILED(
                Bind_ScreenSpaceCB(_postprocessShader, srcWidth, srcHeight),
                logTag);
        };

        bindBloomScreenSpace(viewportWidth, viewportHeight, L"Render_PostProcess:FAIL Bind_ScreenSpaceCB(BloomExtractHalf)");

        // Extract HDR Color -> HalfA
        RETURN_LOG_IF_FAILED(
            GAME->Begin_MRT(MRT::BloomHalfA, NotUseDSV, ClearRTV),
            L"Render_PostProcess:FAIL Begin_MRT(BloomHalfA-Extract)");

        ENDMRT_RETURN_LOG_IF_FAILED(
            GAME->Bind_RT_ShaderResource(_postprocessShader, "g_HDRTexture", RT::HDRColor),
            L"Render_PostProcess:FAIL Bind_RT_ShaderResource(g_HDRTexture-Extract)");

        ENDMRT_RETURN_LOG_IF_FAILED(
            _postprocessShader->Begin(ETOI(PostProcessPass::BloomExtractHalf)),
            L"Render_PostProcess:FAIL ShaderBegin(BloomExtractHalf)");

        ENDMRT_RETURN_LOG_IF_FAILED(
            _viBuffer->Render(),
            L"Render_PostProcess:FAIL RenderRect(BloomExtractHalf)");

        RETURN_LOG_IF_FAILED(
            GAME->End_MRT(),
            L"Render_PostProcess:FAIL End_MRT(BloomHalfA-Extract)");

        Add_RenderCall();

        // DownSample HalfA -> QuarterA
        bindBloomScreenSpace(half.width, half.height, L"Render_PostProcess:FAIL Bind_ScreenSpaceCB(BloomDownsample-Quarter)");
        CHECK_FAILED(Render_BloomPass(
            MRT::BloomQuarterA, RT::BloomHalfA, PostProcessPass::BloomDownsample));

        // DownSample QuarterA -> EighthA
        bindBloomScreenSpace(quarter.width, quarter.height, L"Render_PostProcess:FAIL Bind_ScreenSpaceCB(BloomDownsample-Eighth)");
        CHECK_FAILED(Render_BloomPass(
            MRT::BloomEighthA, RT::BloomQuarterA, PostProcessPass::BloomDownsample));

        // BlurH BloomEighthA -> BloomEighthB
        bindBloomScreenSpace(eighth.width, eighth.height, L"Render_PostProcess:FAIL Bind_ScreenSpaceCB(BloomBlur-EighthH)");
        CHECK_FAILED(Render_BloomPass(
            MRT::BloomEighthB, RT::BloomEighthA, PostProcessPass::BloomBlurH));

        // BlurV BloomEighthB -> BloomEighthA (핑퐁)
        bindBloomScreenSpace(eighth.width, eighth.height, L"Render_PostProcess:FAIL Bind_ScreenSpaceCB(BloomBlur-EighthV)");
        CHECK_FAILED(Render_BloomPass(
            MRT::BloomEighthA, RT::BloomEighthB, PostProcessPass::BloomBlurV));

        // BlurH BloomQuarterA -> BloomQuarterB
        bindBloomScreenSpace(quarter.width, quarter.height, L"Render_PostProcess:FAIL Bind_ScreenSpaceCB(BloomBlur-QuarterH)");
        CHECK_FAILED(Render_BloomPass(
            MRT::BloomQuarterB, RT::BloomQuarterA, PostProcessPass::BloomBlurH));

        // BlurV BloomQuarterB -> BloomQuarterA (핑퐁)
        bindBloomScreenSpace(quarter.width, quarter.height, L"Render_PostProcess:FAIL Bind_ScreenSpaceCB(BloomBlur-QuarterV)");
        CHECK_FAILED(Render_BloomPass(
            MRT::BloomQuarterA, RT::BloomQuarterB, PostProcessPass::BloomBlurV));

        // BlurH BloomHalfB -> BloomHalfA
        bindBloomScreenSpace(half.width, half.height, L"Render_PostProcess:FAIL Bind_ScreenSpaceCB(BloomBlur-HalfH)");
        CHECK_FAILED(Render_BloomPass(
            MRT::BloomHalfB, RT::BloomHalfA, PostProcessPass::BloomBlurH));

        // BlurH BloomHalfA -> BloomHalfB (핑퐁)
        bindBloomScreenSpace(half.width, half.height, L"Render_PostProcess:FAIL Bind_ScreenSpaceCB(BloomBlur-HalfV)");
        CHECK_FAILED(Render_BloomPass(
            MRT::BloomHalfA, RT::BloomHalfB, PostProcessPass::BloomBlurV));

        // UpSample BloomEighthA -> BloomQuarterB
        bindBloomScreenSpace(quarter.width, quarter.height, L"Render_PostProcess:FAIL Bind_ScreenSpaceCB(BloomUpsample-Quarter)");
        CHECK_FAILED(Render_BloomUpsamplePass(
            MRT::BloomQuarterB, RT::BloomEighthA, RT::BloomQuarterA));

        // UpSample BloomQuarterB -> BloomHalfB
        bindBloomScreenSpace(half.width, half.height, L"Render_PostProcess:FAIL Bind_ScreenSpaceCB(BloomUpsample-Half)");
        CHECK_FAILED(Render_BloomUpsamplePass(
            MRT::BloomHalfB, RT::BloomQuarterB, RT::BloomHalfA));
    }

    // Light 기록
    const uint32 bloomFeatureMask = To_Underlying(PostProcessFeature::Bloom);
    if (useBloom)
        params.featureMask |= bloomFeatureMask;
    else
        params.featureMask &= ~bloomFeatureMask;

    RETURN_LOG_IF_FAILED(
        _postprocessShader->Bind_CBufferData(_ppPipeline->Get_PostProcessParams()),
        L"Render_PostProcess:FAIL Bind_CBufferData(ToneMap)");

    RETURN_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_postprocessShader, "g_HDRTexture", RT::HDRColor),
        L"Render_PostProcess:FAIL Bind_RT_ShaderResource(g_HDRTexture-ToneMap)");

    RETURN_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_postprocessShader, "g_BloomTexture", RT::BloomHalfB),
        L"Render_PostProcess:FAIL Bind_RT_ShaderResource(g_BloomTexture-ToneMap)");

    RETURN_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_postprocessShader, "g_BackgroundMaskTexture", RT::BackgroundMask),
        L"Render_PostProcess:FAIL Bind_RT_ShaderResource(g_BackgroundMaskTexture-ToneMap)");

    RETURN_LOG_IF_FAILED(
        _postprocessShader->Begin(ETOI(PostProcessPass::ToneMapping)),
        L"Render_PostProcess:FAIL ShaderBegin(ToneMapping)");

    RETURN_LOG_IF_FAILED(
        _viBuffer->Render(),
        L"Render_PostProcess:FAIL RenderRect(ToneMapping)");

    Add_RenderCall();
}

void Renderer::Render_ScreenFx()
{
    if (!_screenFxShader || !_viBuffer)
        return;

    HRESULT hr = S_OK;

    if (SUCCEEDED(hr))
    {
        hr = _screenFxShader->Bind_ObjectCB(_pipelineWorldMatrix);
        if (FAILED(hr))
            LOG_ERROR("Render_ScreenFx:FAIL Bind_ObjectCB");
    }

    if (SUCCEEDED(hr))
    {
        hr = Bind_ScreenSpaceCB(_screenFxShader, max(1u, _screenSize.width), max(1u, _screenSize.height));
        if (FAILED(hr))
            LOG_ERROR("Render_ScreenFx:FAIL Bind_ScreenSpaceCB");
    }

    if (SUCCEEDED(hr))
    {
        hr = _viBuffer->Bind_Resources();
        if (FAILED(hr)) LOG_ERROR("Render_ScreenFx:FAIL Bind_Resources");
    }

    if (SUCCEEDED(hr))
    {
        hr = GAME->Get_ScreenFxManager()->Render_ScreenFx(_screenFxShader, _viBuffer);
        if (FAILED(hr)) LOG_ERROR("Render_ScreenFx:FAIL Manager->Render_ScreenFx");
    }
}

HRESULT Renderer::Ready_Shaders()
{
    // deferred shader, rect buffer 생성
    _viBuffer = VIBuffer_Rect::Create(_device, _context);
    CHECK_NULL(_viBuffer, E_FAIL);

    _deferredShader = ShaderCom::Create(_device, _context,
        L"../../../Client/Bin/ShaderFiles/Shader_Deferred.hlsl", VTXTEX::Elements, VTXTEX::numElements, ShaderType::Deferred);
    CHECK_NULL(_deferredShader, E_FAIL);

    _postprocessShader = ShaderCom::Create(_device, _context,
        L"../../../Client/Bin/ShaderFiles/Shader_PostProcess.hlsl", VTXTEX::Elements, VTXTEX::numElements, ShaderType::PostProcess);
    CHECK_NULL(_postprocessShader, E_FAIL);

    _screenFxShader = ShaderCom::Create(_device, _context,
        L"../../../Client/Bin/ShaderFiles/Shader_ScreenFx.hlsl", VTXTEX::Elements, VTXTEX::numElements, ShaderType::PostProcess);
    CHECK_NULL(_screenFxShader, E_FAIL);

    _screenSize = {
        GAME->Get_ViewportWidth(), GAME->Get_ViewportHeight()
    };

    // IBL 3분리 자산 로드. 누락 시 nullptr로 남고 셰이더에서 안전 fallback(0 기여) 처리한다.
    _iblIrradianceTexture = Texture::Create(
        _device,
        _context,
        L"../../../Client/Bin/Resources/Textures/Shader/IBL/irradiance_sunset.dds",
        1);

    _iblPrefilterTexture = Texture::Create(
        _device,
        _context,
        L"../../../Client/Bin/Resources/Textures/Shader/IBL/perfilter_sunset.dds",
        1);

    _iblBrdfLutTexture = Texture::Create(
        _device,
        _context,
        L"../../../Client/Bin/Resources/Textures/Shader/IBL/BRDF_LUT.dds",
        1);

    Sync_ViewportDependentState(_screenSize.width, _screenSize.height);

    return S_OK;
}

void Renderer::Sync_ViewportDependentState(uint32 width, uint32 height)
{
    width = max(1u, width);
    height = max(1u, height);

    _pipelineWorldMatrix = Matrix::CreateScale(static_cast<float>(width), static_cast<float>(height), 1.f);
    _PipelineViewMatrix = Matrix::Identity;
    _pipelineProjMatrix = XMMatrixOrthographicLH(static_cast<float>(width), static_cast<float>(height), 0.f, 1.f);

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

HRESULT Renderer::Bind_ScreenSpaceCB(Shared<ShaderCom> shader, uint32 width, uint32 height) const
{
    CHECK_NULL(shader, E_FAIL);

    Shared<Camera> activeCamera = GAME->Get_ActiveCamera();
    if (activeCamera == nullptr)
        return E_FAIL;

    ScreenSpaceCB screenSpaceCB{};
    screenSpaceCB.screenViewMatrix = _PipelineViewMatrix;
    screenSpaceCB.screenProjMatrix = _pipelineProjMatrix;

    screenSpaceCB.viewInverseMatrix = *GAME->Get_Transform_Inverse(D3DTS::View);
    screenSpaceCB.projInverseMatrix = *GAME->Get_Transform_Inverse(D3DTS::Proj);

    screenSpaceCB.cameraPosition = *GAME->Get_CamPosition();
    screenSpaceCB.farPlane = activeCamera->Get_Far();

    const uint32 resolvedWidth = max(1u, (width == 0u) ? _screenSize.width : width);
    const uint32 resolvedHeight = max(1u, (height == 0u) ? _screenSize.height : height);

    screenSpaceCB.texelSize = Vec2{
        1.f / static_cast<float>(resolvedWidth),
        1.f / static_cast<float>(resolvedHeight)
    };
    screenSpaceCB.screenSize = Vec2{
        static_cast<float>(resolvedWidth),
        static_cast<float>(resolvedHeight)
    };

    return shader->Bind_CBufferData(screenSpaceCB);
}

HRESULT Renderer::Render_BloomPass(const wchar_t* dstMrt, const wchar_t* srcRt, PostProcessPass pass)
{
    const wchar_t* passName = L"Unknown";
    switch (pass)
    {
    case PostProcessPass::BloomDownsample: passName = L"BloomDownsample"; break;
    case PostProcessPass::BloomBlurH:      passName = L"BloomBlurH"; break;
    case PostProcessPass::BloomBlurV:      passName = L"BloomBlurV"; break;
    default: break;
    }

    const wstring beginLog = wstring(L"Render_PostProcess:FAIL Begin_MRT(") + dstMrt + L"," + passName + L")";
    const wstring bindLog = wstring(L"Render_PostProcess:FAIL Bind_RT_ShaderResource(g_BloomTexture:") + srcRt + L"," + passName + L")";
    const wstring shaderLog = wstring(L"Render_PostProcess:FAIL ShaderBegin(") + passName + L")";
    const wstring renderLog = wstring(L"Render_PostProcess:FAIL RenderRect(") + passName + L")";
    const wstring endLog = wstring(L"Render_PostProcess:FAIL End_MRT(") + dstMrt + L"," + passName + L")";

    RETURN_FAIL_LOG_IF_FAILED(
        GAME->Begin_MRT(dstMrt, NotUseDSV, ClearRTV),
        beginLog.c_str());

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_postprocessShader, "g_BloomTexture", srcRt),
        bindLog.c_str());

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        _postprocessShader->Begin(ETOI(pass)),
        shaderLog.c_str());

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        _viBuffer->Render(),
        renderLog.c_str());

    RETURN_FAIL_LOG_IF_FAILED(
        GAME->End_MRT(),
        endLog.c_str());

    Add_RenderCall();

    return S_OK;
}


HRESULT Renderer::Render_BloomUpsamplePass(const wchar_t* dstMrt, const wchar_t* smallRt, const wchar_t* baseRt)
{
    constexpr const wchar_t* passName = L"BloomUpsampleAdd";
    const wstring beginLog = wstring(L"Render_PostProcess:FAIL Begin_MRT(") + dstMrt + L"," + passName + L")";
    const wstring smallBindLog = wstring(L"Render_PostProcess:FAIL Bind_RT_ShaderResource(g_BloomTexture:") + smallRt + L"," + passName + L")";
    const wstring baseBindLog = wstring(L"Render_PostProcess:FAIL Bind_RT_ShaderResource(g_BloomBaseTexture:") + baseRt + L"," + passName + L")";
    const wstring shaderLog = wstring(L"Render_PostProcess:FAIL ShaderBegin(") + passName + L")";
    const wstring renderLog = wstring(L"Render_PostProcess:FAIL RenderRect(") + passName + L")";
    const wstring endLog = wstring(L"Render_PostProcess:FAIL End_MRT(") + dstMrt + L"," + passName + L")";

    RETURN_FAIL_LOG_IF_FAILED(
        GAME->Begin_MRT(dstMrt, NotUseDSV, ClearRTV),
        beginLog.c_str());

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_postprocessShader, "g_BloomTexture", smallRt),
        smallBindLog.c_str());

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_postprocessShader, "g_BloomBaseTexture", baseRt),
        baseBindLog.c_str());

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        _postprocessShader->Begin(ETOI(PostProcessPass::BloomUpsampleAdd)),
        shaderLog.c_str());

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        _viBuffer->Render(),
        renderLog.c_str());

    RETURN_FAIL_LOG_IF_FAILED(
        GAME->End_MRT(),
        endLog.c_str());

    Add_RenderCall();

    return S_OK;
}

HRESULT Renderer::Render_DistortionResolve()
{
    if (!_postprocessShader || !_viBuffer)
        return E_FAIL;

    RETURN_FAIL_LOG_IF_FAILED(
        _postprocessShader->Bind_ObjectCB(_pipelineWorldMatrix),
        L"Render_DistortionResolve:FAIL Bind_ObjectCB");

    RETURN_FAIL_LOG_IF_FAILED(
        Bind_ScreenSpaceCB(_postprocessShader, max(1u, _screenSize.width), max(1u, _screenSize.height)),
        L"Render_DistortionResolve:FAIL Bind_ScreenSpaceCB");

    RETURN_FAIL_LOG_IF_FAILED(
        _viBuffer->Bind_Resources(),
        L"Render_DistortionResolve:FAIL Bind_Resources");

    RETURN_FAIL_LOG_IF_FAILED(
        GAME->Begin_MRT(MRT::HDR, NotUseDSV, NotClearRTV),
        L"Render_DistortionResolve:FAIL Begin_MRT(HDR)");

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_postprocessShader, "g_DistortionSourceTexture", RT::DistortionSource),
        L"Render_DistortionResolve:FAIL Bind_RT_ShaderResource(g_DistortionSourceTexture)");

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_postprocessShader, "g_DistortionFieldTexture", RT::DistortionField),
        L"Render_DistortionResolve:FAIL Bind_RT_ShaderResource(g_DistortionFieldTexture)");

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        _postprocessShader->Begin(ETOI(PostProcessPass::DistortionResolve)),
        L"Render_DistortionResolve:FAIL ShaderBegin(DistortionResolve)");

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        _viBuffer->Render(),
        L"Render_DistortionResolve:FAIL RenderRect");

    RETURN_FAIL_LOG_IF_FAILED(
        GAME->End_MRT(),
        L"Render_DistortionResolve:FAIL End_MRT(HDR)");

    Add_RenderCall();

    return S_OK;
}

void Renderer::Update_DynamicShadowCamera()
{
    const ShadowLightDesc* shadowDesc = GAME->Get_ShadowLightDesc();
    if (shadowDesc == nullptr || shadowDesc->followCameraFrustum == false)
        return;

    Shared<Camera> activeCamera = GAME->Get_ActiveCamera();
    if (activeCamera == nullptr)
        return;

    const LIGHT_DESC* lightDesc = GAME->Get_PrimaryShadowLightDesc();
    if (lightDesc == nullptr)
        return;

    ShadowLightDesc updatedDesc = *shadowDesc;
    updatedDesc.shadowMapWidth = _shadowMapSize.width;
    updatedDesc.shadowMapHeight = _shadowMapSize.height;

    const Vec3 lightDir = Vec3(
        lightDesc->direction.x,
        lightDesc->direction.y,
        lightDesc->direction.z
    );

    GAME->Update_ShadowFromCameraFrustum(updatedDesc, activeCamera, lightDir);
}

void Renderer::Render_StaticShadowOnce()
{
    ENGINE_PROFILE_SCOPE_DETAIL("Render::StaticShadow");

    if (!_staticShadowDirty)
        return;

    BoundingBox staticBounds{};
    if (!Build_StaticShadowBounds(staticBounds))
        return;

    Update_StaticShadowCamera(staticBounds);

    if (FAILED(GAME->Begin_MRT(MRT::StaticShadowObjects, UseDSV, ClearRTV)))
        return;

    const uint32 originWidth = _screenSize.width;
    const uint32 originHeight = _screenSize.height;

    Change_ViewportDesc(_shadowStaticSize.width, _shadowStaticSize.height);

    for (const auto& weakObject : _staticShadowCasters)
    {
        Shared<GameObject> gameObject = weakObject.lock();
        if (gameObject == nullptr || !gameObject->Casts_StaticShadow())
            continue;

        if (gameObject->Get_Transform() == nullptr)
            continue;

        gameObject->Render_Shadow();
        Add_RenderCall();
    }

    CHECK_FAILED(GAME->End_MRT());

    Change_ViewportDesc(originWidth, originHeight);

    _staticShadowDirty = false;
    _staticShadowReady = true;
}

void Renderer::Render_DynamicShadow()
{
    ENGINE_PROFILE_SCOPE_DETAIL("Render::DynamicShadow");

    Update_DynamicShadowCamera();

    if (FAILED(GAME->Begin_MRT(MRT::ShadowObjects, UseDSV, ClearRTV)))
        return;

    const uint32 originWidth = _screenSize.width;
    const uint32 originHeight = _screenSize.height;

    Change_ViewportDesc(_shadowMapSize.width, _shadowMapSize.height);

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

bool Renderer::Build_StaticShadowBounds(BoundingBox& outBounds) const
{
    bool hasBounds = false;

    for (const auto& weakObject : _staticShadowCasters)
    {
        Shared<GameObject> gameObject = weakObject.lock();
        if (gameObject == nullptr || !gameObject->Casts_StaticShadow())
            continue;

        if (gameObject->Get_Transform() == nullptr)
            continue;

        BoundingBox objectBounds{};
        if (!gameObject->Try_Get_ShadowWorldBounds(objectBounds))
            continue;

        if (!hasBounds)
        {
            outBounds = objectBounds;
            hasBounds = true;
        }
        else
        {
            BoundingBox::CreateMerged(outBounds, outBounds, objectBounds);
        }
    }

    return hasBounds;
}

void Renderer::Update_StaticShadowCamera(const BoundingBox& bounds)
{
    const LIGHT_DESC* lightDesc = GAME->Get_PrimaryShadowLightDesc();
    const ShadowLightDesc* shadowDesc = GAME->Get_ShadowLightDesc();
    if (lightDesc == nullptr || shadowDesc == nullptr)
        return;

    Vec3 lightDir(
        lightDesc->direction.x,
        lightDesc->direction.y,
        lightDesc->direction.z);

    if (lightDir.LengthSquared() < 0.0001f)
        lightDir = Vec3(1.f, -1.f, -1.f);

    lightDir.Normalize();

    GAME->Update_StaticShadowFromBounds(*shadowDesc, bounds, lightDir);
}

void Renderer::Register_StaticShadowCaster(const Shared<GameObject>& gameObject)
{
    if (gameObject == nullptr || !gameObject->Casts_StaticShadow()) // Static 인지 체크
        return;

    _staticShadowCasters.push_back(gameObject);

    Mark_StaticShadowDirty();
}

void Renderer::Mark_StaticShadowDirty()
{
     _staticShadowDirty = true;
}

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

#ifdef _DEBUG

void Renderer::DumpCurrentOMState(const wchar_t* stageTag) const
{
    return; // 컴파일러가 호출 자체 무시할 거임
    if (false == _useRTVDump || nullptr == _context)
        return;

    ComPtr<ID3D11RenderTargetView> currentRTV;
    ComPtr<ID3D11DepthStencilView> currentDSV;
    _context->OMGetRenderTargets(1, currentRTV.GetAddressOf(), currentDSV.GetAddressOf());

    wstring rtvLabel = L"nullptr";
    if (currentRTV)
    {
        wstring rtvName;
        if (Try_GetObjectName(currentRTV.Get(), rtvName))
            rtvLabel = rtvName;
        else
            rtvLabel = PtrToWString(currentRTV.Get());
    }

    wstring dsvLabel = L"nullptr";
    if (currentDSV)
    {
        wstring dsvName;
        if (Try_GetObjectName(currentDSV.Get(), dsvName))
            dsvLabel = dsvName;
        else
            dsvLabel = PtrToWString(currentDSV.Get());
    }

    const wchar_t* safeStageTag = stageTag ? stageTag : L"UnknownStage";
    wstring message = L"[OM][Stage=";
    message += safeStageTag;
    message += L"] RTV=";
    message += rtvLabel;
    message += L" DSV=";
    message += dsvLabel;
    message += L"\n";

    OutputDebugStringW(message.c_str());
}

bool Renderer::Try_ConsumePerfSummary(const float windowSec, RenderPerfSummary& outSummary)
{
    outSummary = {};

    if (windowSec <= 0.f || 0 == _renderPerfWindow.render.sampleCount)
        return false;

    outSummary.valid = true;
    outSummary.renderCallCount = static_cast<uint32>(_renderPerfWindow.render.sampleCount);

    outSummary.renderAvgMs = static_cast<float>(
        LogUtils::Safe_Divide(_renderPerfWindow.render.accumulatedMs, static_cast<double>(_renderPerfWindow.render.sampleCount)));
    outSummary.renderMaxMs = static_cast<float>(_renderPerfWindow.render.maxMs);

    outSummary.priorityAvgMs = static_cast<float>(
        LogUtils::Safe_Divide(_renderPerfWindow.priority.accumulatedMs, static_cast<double>(_renderPerfWindow.priority.sampleCount)));
    outSummary.priorityMaxMs = static_cast<float>(_renderPerfWindow.priority.maxMs);

    outSummary.shadowAvgMs = static_cast<float>(
        LogUtils::Safe_Divide(_renderPerfWindow.shadow.accumulatedMs, static_cast<double>(_renderPerfWindow.shadow.sampleCount)));
    outSummary.shadowMaxMs = static_cast<float>(_renderPerfWindow.shadow.maxMs);

    outSummary.nonBlendAvgMs = static_cast<float>(
        LogUtils::Safe_Divide(_renderPerfWindow.nonBlend.accumulatedMs, static_cast<double>(_renderPerfWindow.nonBlend.sampleCount)));
    outSummary.nonBlendMaxMs = static_cast<float>(_renderPerfWindow.nonBlend.maxMs);

    outSummary.lightsAvgMs = static_cast<float>(
        LogUtils::Safe_Divide(_renderPerfWindow.lights.accumulatedMs, static_cast<double>(_renderPerfWindow.lights.sampleCount)));
    outSummary.lightsMaxMs = static_cast<float>(_renderPerfWindow.lights.maxMs);

    outSummary.combinedHDRAvgMs = static_cast<float>(
        LogUtils::Safe_Divide(_renderPerfWindow.combinedHDR.accumulatedMs, static_cast<double>(_renderPerfWindow.combinedHDR.sampleCount)));
    outSummary.combinedHDRMaxMs = static_cast<float>(_renderPerfWindow.combinedHDR.maxMs);

    outSummary.nonLightAvgMs = static_cast<float>(
        LogUtils::Safe_Divide(_renderPerfWindow.nonLight.accumulatedMs, static_cast<double>(_renderPerfWindow.nonLight.sampleCount)));
    outSummary.nonLightMaxMs = static_cast<float>(_renderPerfWindow.nonLight.maxMs);

    outSummary.blendAvgMs = static_cast<float>(
        LogUtils::Safe_Divide(_renderPerfWindow.blend.accumulatedMs, static_cast<double>(_renderPerfWindow.blend.sampleCount)));
    outSummary.blendMaxMs = static_cast<float>(_renderPerfWindow.blend.maxMs);

    outSummary.uiAvgMs = static_cast<float>(
        LogUtils::Safe_Divide(_renderPerfWindow.ui.accumulatedMs, static_cast<double>(_renderPerfWindow.ui.sampleCount)));
    outSummary.uiMaxMs = static_cast<float>(_renderPerfWindow.ui.maxMs);

    outSummary.debugAvgMs = static_cast<float>(
        LogUtils::Safe_Divide(_renderPerfWindow.debug.accumulatedMs, static_cast<double>(_renderPerfWindow.debug.sampleCount)));
    outSummary.debugMaxMs = static_cast<float>(_renderPerfWindow.debug.maxMs);

    _renderPerfWindow = {}; // Reset_RenderPerfWindow
    return true;
}

void Renderer::Accumulate_RenderPerf(
    double renderMs,
    double priorityMs,
    double shadowMs,
    double nonBlendMs,
    double lightsMs,
    double combinedHDRMs,
    double postprocessMs,
    double nonLightMs,
    double blendMs,
    double uiMs,
    double debugMs)
{
    LogUtils::Accumulate_PassPerf(_renderPerfWindow.render, renderMs);
    LogUtils::Accumulate_PassPerf(_renderPerfWindow.priority, priorityMs);
    LogUtils::Accumulate_PassPerf(_renderPerfWindow.shadow, shadowMs);
    LogUtils::Accumulate_PassPerf(_renderPerfWindow.nonBlend, nonBlendMs);
    LogUtils::Accumulate_PassPerf(_renderPerfWindow.lights, lightsMs);
    LogUtils::Accumulate_PassPerf(_renderPerfWindow.combinedHDR, combinedHDRMs);
    LogUtils::Accumulate_PassPerf(_renderPerfWindow.postProcess, postprocessMs);
    LogUtils::Accumulate_PassPerf(_renderPerfWindow.nonLight, nonLightMs);
    LogUtils::Accumulate_PassPerf(_renderPerfWindow.blend, blendMs);
    LogUtils::Accumulate_PassPerf(_renderPerfWindow.ui, uiMs);
    LogUtils::Accumulate_PassPerf(_renderPerfWindow.debug, debugMs);
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
    {
        _renderGroups[i].clear();
        _renderGroupBackup[i].clear();
    }
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

    if (GAME->Get_ActiveCamera() == nullptr)
        return;

    _debugComponents.clear();

    // 디버그 RT 프리뷰는 ScreenSpace 셰이더의 VS_MAIN(= g_ViewMatrix/g_ProjMatrix 사용)로 그린다.
    // 다른 패스/리사이즈로 값이 바뀌면 화면 밖으로 날아갈 수 있으니 여기서 매번 명시적으로 바인딩한다.
    if (_deferredShader)
    {
        const uint32 width = _screenSize.width;
        const uint32 height = _screenSize.height;

        _PipelineViewMatrix = Matrix::Identity;
        _pipelineProjMatrix = XMMatrixOrthographicLH(static_cast<float>(width), static_cast<float>(height), 0.f, 1.f);

        CHECK_FAILED(Bind_ScreenSpaceCB(_deferredShader));
    }

    CHECK_FAILED(GAME->Render_RT_Debug(_viBuffer, _deferredShader));
    Add_RenderCall();
}

#endif

#ifdef ENDMRT_RETURN_IF_FAILED
#undef ENDMRT_RETURN_IF_FAILED
#endif
#ifdef ENDMRT_RETURN_LOG_IF_FAILED
#undef ENDMRT_RETURN_LOG_IF_FAILED
#endif
#ifdef COMBINEDHDR_RETURN_LOG_IF_FAILED
#undef COMBINEDHDR_RETURN_LOG_IF_FAILED
#endif
#ifdef ENDMRT_RETURN_FAIL_LOG_IF_FAILED
#undef ENDMRT_RETURN_FAIL_LOG_IF_FAILED
#endif
#ifdef RETURN_FAIL_LOG_IF_FAILED
#undef RETURN_FAIL_LOG_IF_FAILED
#endif
