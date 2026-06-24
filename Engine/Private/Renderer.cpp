#include "Renderer.h"
#include "Camera.h"
#include "EffectEmitter.h"
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

    if (_ppPipeline)
        _ppPipeline->Update(GAME->Get_WorldTimeDelta());

    Change_ViewportDesc(_screenSize.width, _screenSize.height);
    Reset_RenderCall();

#ifdef _DEBUG
    DumpCurrentOMState(L"Renderer::Draw:Begin");

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

    if (_renderEnvironment)
    {
        Render_Fog(); // 에디터 프리뷰는 현재 월드의 fog volume을 상속하지 않는다.
        Render_VolumetricLightShaft();
    }

    // ========================================= //
    Render_NonLight();
    // ========================================= //
#ifdef _DEBUG
    DumpCurrentOMState(L"Render_NonLight:After");
    DumpCurrentOMState(L"Render_EffectMasked:Before");
#endif

    // ========================================= //
    Render_EffectMasked();
    // ========================================= //
#ifdef _DEBUG
    DumpCurrentOMState(L"Render_EffectMasked:After");
    nonlightMs = chrono::duration<double, milli>(chrono::steady_clock::now() - nonlightBegin).count();
    const auto blendBegin = chrono::steady_clock::now();
    DumpCurrentOMState(L"Render_Blend:Before");
#endif

    // ========================================= //
    Render_Blend();
    // ========================================= //
#ifdef _DEBUG
    DumpCurrentOMState(L"Render_Blend:After");
    blendMs = chrono::duration<double, milli>(chrono::steady_clock::now() - blendBegin).count();
    DumpCurrentOMState(L"Render_Distortion:Before");
#endif

    // ========================================= //
    Render_Distortion();
    // ========================================= //
#ifdef _DEBUG
    DumpCurrentOMState(L"Render_Distortion:After");
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

#ifdef _DEBUG
    DumpCurrentOMState(L"Render_ScreenFX:After");
    uiMs = chrono::duration<double, milli>(chrono::steady_clock::now() - uiBegin).count();
    const auto debugBegin = chrono::steady_clock::now();
#endif

    const ScreenFx_Manager* screenFxManager = GAME->Get_ScreenFxManager();
    const bool useScreenFx = screenFxManager && screenFxManager->Is_ScreenFXActivated() && screenFxManager->Has_ActiveEffects() && _screenFxShader;
    if (useScreenFx)
    {
        Render_ScreenFx();
    }

    if (screenFxManager && screenFxManager->Is_CaptureRequested())
    {
        GAME->Bind_BackBuffer();
        GAME->Copy_CurrentRTV_To(RT::ScreenCapture);

        const_cast<ScreenFx_Manager*>(screenFxManager)->Clear_CaptureRequested();
    }


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

    // 그림자를 그리지 않는 아래의 경우, shadow MRT를 한 번씩 열고 닫아 비운다.
    if (_shadowEnabled == false)
    {
        const uint32 cascadeCount = max(1u, GAME->Get_ShadowCascadeCount());

        for (uint32 cascadeIndex = 0; cascadeIndex < cascadeCount; ++cascadeIndex)
        {
            if (FAILED(GAME->Begin_MRT(MRT::ShadowObjects, UseDSV, ClearRTV, cascadeIndex)))
                continue;

            CHECK_FAILED(GAME->End_MRT());
        }

        _renderGroups[ETOI(RenderGroup::Shadow)].clear();
        GAME->Set_ActiveShadowCascade(0);
#ifdef _DEBUG
        Sync_ShadowDebugTargets();
#endif
        return;
    }

    const ShadowLightDesc* shadowDesc = GAME->Get_ShadowLightDesc();
    if (shadowDesc != nullptr && !shadowDesc->enabled)
    {
        const uint32 cascadeCount = max(1u, GAME->Get_ShadowCascadeCount());

        for (uint32 cascadeIndex = 0; cascadeIndex < cascadeCount; ++cascadeIndex)
        {
            if (FAILED(GAME->Begin_MRT(MRT::ShadowObjects, UseDSV, ClearRTV, cascadeIndex)))
                continue;

            CHECK_FAILED(GAME->End_MRT());
        }

        _renderGroups[ETOI(RenderGroup::Shadow)].clear();
        GAME->Set_ActiveShadowCascade(0);
#ifdef _DEBUG
        Sync_ShadowDebugTargets();
#endif
        return;
    }

    Render_DynamicShadow();
#ifdef _DEBUG
    Sync_ShadowDebugTargets();
#endif
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

    DeferredLightingCB deferredLightingParams{};
    if (_lightingPipeline)
        deferredLightingParams = _lightingPipeline->Get_DeferredLightingParams();

    ENDMRT_RETURN_IF_FAILED(
        _deferredShader->Bind_CBufferData(deferredLightingParams));

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

    COMBINEDHDR_RETURN_LOG_IF_FAILED(
        GAME->Bind_CameraCB(_deferredShader),
        L"Render_CombinedHDR:FAIL Bind_CameraCB");

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

void Renderer::Render_Fog()
{
    if (!Is_RenderEnvironment())
        return;

    if (!_deferredShader || !_viBuffer)
        return;

    const Vec4* camPosition = GAME->Get_CamPosition();
    if (camPosition == nullptr)
        return;

    FogZoneCB fogZoneCB = GAME->Build_ActiveFogZones(Vec3(camPosition->x, camPosition->y, camPosition->z));
    if (fogZoneCB.aabbCount + fogZoneCB.obbCount == 0)
        return;

    RETURN_LOG_IF_FAILED(
        GAME->Begin_MRT(MRT::HDR, NotUseDSV, NotClearRTV),
        L"Render_Fog:FAIL Begin_MRT(HDR)");

    ENDMRT_RETURN_LOG_IF_FAILED(
        _deferredShader->Bind_ObjectCB(_pipelineWorldMatrix),
        L"Render_Fog:FAIL Bind_ObjectCB");

    ENDMRT_RETURN_LOG_IF_FAILED(
        GAME->Bind_PrimaryShadowLightCB(_deferredShader),
        L"Render_Fog:FAIL Bind_PrimaryShadowLightCB");

    ENDMRT_RETURN_LOG_IF_FAILED(
        GAME->Bind_CameraCB(_deferredShader),
        L"Render_Fog:FAIL Bind_CameraCB");

    ENDMRT_RETURN_LOG_IF_FAILED(
        GAME->Bind_ShadowCB(_deferredShader),
        L"Render_Fog:FAIL Bind_ShadowCB");

    ENDMRT_RETURN_LOG_IF_FAILED(
        _deferredShader->Bind_ObjectCB(_pipelineWorldMatrix),
        L"Render_Fog:FAIL Bind_ObjectCB(Repeat)");

    ENDMRT_RETURN_LOG_IF_FAILED(
        Bind_ScreenSpaceCB(_deferredShader),
        L"Render_Fog:FAIL Bind_ScreenSpaceCB");

    ENDMRT_RETURN_LOG_IF_FAILED(
        _deferredShader->Bind_CBufferData(fogZoneCB),
        L"Render_Fog:FAIL Bind_CBufferData(FogZoneCB)");

    ENDMRT_RETURN_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_deferredShader, "g_DepthTexture", RT::Depth),
        L"Render_Fog:FAIL Bind_RT_ShaderResource(g_DepthTexture)");

    ENDMRT_RETURN_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_deferredShader, "g_LightDepthTexture", RT::LightDepth),
        L"Render_Fog:FAIL Bind_RT_ShaderResource(g_LightDepthTexture)");

    ENDMRT_RETURN_LOG_IF_FAILED(
        _viBuffer->Bind_Resources(),
        L"Render_Fog:FAIL Bind_Resources");

    auto* noiseTexture = GAME->Get_SRV("T_Random_56");
    if FAILED(_deferredShader->Bind_SRV("g_FogNoiseTexture", noiseTexture))
        LOG_WARN("Failed to bind noise texutre!");

    const float fDT = 0.016f;
    static float s_elapsed = 0.f;
    s_elapsed += fDT;
    if FAILED(_deferredShader->Bind_RawValue("g_fDT", &s_elapsed, sizeof(float)))
        LOG_WARN("Failed to bind noise texutre!");


    ENDMRT_RETURN_LOG_IF_FAILED(
        _deferredShader->Begin(ETOI(DeferredPass::Fog)),
        L"Render_Fog:FAIL ShaderBegin(Fog)");

    ENDMRT_RETURN_LOG_IF_FAILED(
        _viBuffer->Render(),
        L"Render_Fog:FAIL RenderRect");

    GAME->End_MRT();
    Add_RenderCall();
}

void Renderer::Render_VolumetricLightShaft()
{
    if (!_lightingPipeline)
        return ;

    _lightingPipeline->Invalidate_FroxelFrameData();

    Shared<Camera> activeCamera = GAME->Get_ActiveCamera();

    const Matrix* view = GAME->Get_Transform(D3DTS::View);
    const Matrix* projection = GAME->Get_Transform(D3DTS::Proj);
    const Matrix* inverseView = GAME->Get_Transform_Inverse(D3DTS::View);
    const Matrix* inverseProjection = GAME->Get_Transform_Inverse(D3DTS::Proj);

    const LIGHT_DESC* lightDesc = GAME->Get_PrimaryShadowLightDesc();
    const ShadowLightDesc* shadowLightDesc = GAME->Get_ShadowLightDesc();
    if (lightDesc == nullptr || lightDesc->type != LightType::Directional)
        return ;

    const Vec4* camPosition = GAME->Get_CamPosition();
    if (activeCamera == nullptr || view == nullptr || projection == nullptr ||
        inverseView == nullptr || inverseProjection == nullptr || camPosition == nullptr)
        return;

    static bool hasFroxelHistory = false;
    static Matrix previousFroxelView{};
    static Matrix previousFroxelProjection{};

    FroxelCB froxelCB{};
    froxelCB.froxelWidth = _lightingPipeline->Get_DebugFroxelWidth();
    froxelCB.froxelHeight = _lightingPipeline->Get_DebugFroxelHeight();
    froxelCB.froxelDepth = _lightingPipeline->Get_DebugFroxelDepth();
    froxelCB.froxelNear = activeCamera->Get_Near();
    froxelCB.froxelFar = (shadowLightDesc != nullptr && shadowLightDesc->zf > froxelCB.froxelNear)
        ? shadowLightDesc->zf
        : activeCamera->Get_Far();
    froxelCB.compositeIntensity = 0.35f;
    froxelCB.depthBias = 0.05f;
    froxelCB.shadowStrength = 1.f;
    froxelCB.cameraFar = activeCamera->Get_Far();

    froxelCB.inverseView = *inverseView;
    froxelCB.inverseProjection = *inverseProjection;
    froxelCB.previousView = hasFroxelHistory ? previousFroxelView : *view;
    froxelCB.previousProjection = hasFroxelHistory ? previousFroxelProjection : *projection;

    froxelCB.lightDirection = lightDesc->direction;
    froxelCB.lightColor = lightDesc->diffuse;

    froxelCB.camPosition = *GAME->Get_CamPosition();
    froxelCB.temporalBlend = hasFroxelHistory ? 0.65f : 0.f;
    froxelCB.jitterZ = 0.f;
    froxelCB.temporalDepthReject = 0.25f;
    froxelCB.historyValid = hasFroxelHistory ? 1u : 0u;

    ShadowCB shadowCB{};
    if (FAILED(GAME->Get_ShadowCBSnapshot(shadowCB)))
    {
        LOG_WARN("Render_VolumetricLightShaft:FAIL Get_ShadowCBSnapshot");
        return;
    }

    _lightingPipeline->Bind_FroxelInputSRVs(
        GAME->Get_RT_SRV(RT::Depth),
        GAME->Get_RT_SRV(RT::LightDepth));

    VolumetricLightShaftZoneCB lightShaftZoneCB = GAME->Build_ActiveVolumetricLightShaftZones(
        Vec3(camPosition->x, camPosition->y, camPosition->z));
    if(FAILED(_lightingPipeline->Update_FroxelZone(lightShaftZoneCB)))
    {
        LOG_WARN("Render_VolumetricLightShaft:FAIL Update_FroxelZone");
        return;
    }

    if(FAILED(_lightingPipeline->Update_Froxel(froxelCB)))
    {
        LOG_WARN("Render_VolumetricLightShaft:FAIL Update_Froxel");
        return;
    }

    if (FAILED(_lightingPipeline->Update_FroxelShadow(shadowCB)))
    {
        LOG_WARN("Render_VolumetricLightShaft:FAIL Update_FroxelShadow");
        return;
    }

    if(FAILED(_lightingPipeline->Dispatch_FroxelInject()))
    {
        LOG_WARN("Render_VolumetricLightShaft:FAIL Dispatch_FroxelInject");
        return;
    }

    if (FAILED(_lightingPipeline->Dispatch_FroxelBlur()))
    {
        LOG_WARN("Render_VolumetricLightShaft:FAIL Dispatch_FroxelBlur");
        return;
    }

    if (FAILED(_lightingPipeline->Dispatch_FroxelAccumulate()))
    {
        LOG_WARN("Render_VolumetricLightShaft:FAIL Dispatch_FroxelAccumulate");
        return;
    }

    if (FAILED(_lightingPipeline->Dispatch_FroxelTemporal()))
    {
        LOG_WARN("Render_VolumetricLightShaft:FAIL Dispatch_FroxelTemporal");
        return;
    }

    previousFroxelView = *view;
    previousFroxelProjection = *projection;
    hasFroxelHistory = true;
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

void Renderer::Render_EffectMasked()
{
    auto& maskedGroup = _renderGroups[ETOI(RenderGroup::EffectMasked)];
    if (maskedGroup.empty())
        return;

    RETURN_LOG_IF_FAILED(
        GAME->Begin_MRT(MRT::HDR, UseDSV, NotClearRTV),
        L"Render_EffectMasked:FAIL Begin_MRT(HDR)");

    _currentGroup = RenderGroup::EffectMasked;

    for (auto& renderObject : maskedGroup)
    {
        Shared<GameObject> gameObject = renderObject.lock();
        if (gameObject != nullptr)
        {
            gameObject->Render();
            Add_RenderCall();
        }
    }

    maskedGroup.clear();

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

    auto& blendObjects = _renderGroups[ETOI(RenderGroup::Blend)];
    const Vec4* cameraPosition4 = GAME->Get_CamPosition();
    Shared<Camera> activeCamera = GAME->Get_ActiveCamera();
    Shared<TransformCom> cameraTransform = activeCamera != nullptr ? activeCamera->Get_Transform() : nullptr;
    if (cameraPosition4 != nullptr && cameraTransform != nullptr)
    {
        constexpr float kBlendSortBiasScale = 1.f;
        const Vec3 cameraPosition{ cameraPosition4->x, cameraPosition4->y, cameraPosition4->z };
        Vec3 cameraForward = cameraTransform->Get_WorldForward();
        cameraForward.Normalize();

        struct BlendSortInfo
        {
            bool valid{ false };
            int32 layer{ 0 };
            EffectSortPolicy policy{ EffectSortPolicy::EmitterDepth };
            float depth{ 0.f };
        };

        const auto resolve_sort_info = [&](const Weak<GameObject>& renderObject) -> BlendSortInfo
        {
            BlendSortInfo info{};
            const Shared<GameObject> gameObject = renderObject.lock();
            if (gameObject == nullptr)
                return info;

            Vec3 objectWorldPosition{};
            float artistSortBias = 0.f;
            const Shared<EffectEmitter> effectEmitter = dynamic_pointer_cast<EffectEmitter>(gameObject);
            if (effectEmitter != nullptr)
            {
                if (!effectEmitter->Try_Get_BlendSortWorldPosition(objectWorldPosition))
                    return info;

                info.layer = effectEmitter->Get_BlendSortLayer();
                info.policy = effectEmitter->Get_BlendSortPolicy();
                artistSortBias = effectEmitter->Get_BlendSortBias();
            }
            else
            {
                const Shared<TransformCom> transform = gameObject->Get_Transform();
                if (transform == nullptr)
                    return info;

                objectWorldPosition = transform->Get_WorldPosition();
            }

            if (info.policy == EffectSortPolicy::EmitterDepth)
                info.depth = (objectWorldPosition - cameraPosition).Dot(cameraForward) - artistSortBias * kBlendSortBiasScale;
            info.valid = true;
            return info;
        };

        // 낮은 layer를 먼저 그리고 같은 layer 안에서만 view-depth 정렬을 적용한다.
        blendObjects.sort([&](const Weak<GameObject>& lhs, const Weak<GameObject>& rhs)
            {
                const BlendSortInfo lhsInfo = resolve_sort_info(lhs);
                const BlendSortInfo rhsInfo = resolve_sort_info(rhs);

                if (!lhsInfo.valid || !rhsInfo.valid)
                    return false;

                if (lhsInfo.layer != rhsInfo.layer)
                    return lhsInfo.layer < rhsInfo.layer;

                if (lhsInfo.policy != EffectSortPolicy::EmitterDepth ||
                    rhsInfo.policy != EffectSortPolicy::EmitterDepth ||
                    lhsInfo.depth == rhsInfo.depth)
                {
                    return false;
                }

                return lhsInfo.depth > rhsInfo.depth;
            });
    }

    for (auto& renderObject : blendObjects)
    {
        Shared<GameObject> gameObject = renderObject.lock();
        if (nullptr != gameObject)
        {
            gameObject->Render();
            Add_RenderCall();
        }
    }

    blendObjects.clear();

    GAME->End_MRT();
}

void Renderer::Render_UIEffect()
{
    GAME->Bind_BackBuffer();
    _currentGroup = RenderGroup::UIEffect;

    if (auto activeCamera = GAME->Get_ActiveCamera())
        activeCamera->Apply_View();

    for (auto& renderObject : _renderGroups[ETOI(RenderGroup::UIEffect)])
    {
        Shared<GameObject> gameObject = renderObject.lock();
        if (nullptr != gameObject)
        {
            gameObject->Render();
            Add_RenderCall();
        }
    }

    _renderGroups[ETOI(RenderGroup::UIEffect)].clear();
}

void Renderer::PostDraw()
{
    Render_UIEffect();
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

    // ==============================God Ray ========================================
    LightShaftCB lightShaftParams = _ppPipeline->Get_LightShaftParams();
    const bool useLightShaft = Try_BuildLightShaftParams(lightShaftParams);
    _ppPipeline->Set_LightShaftParams(lightShaftParams);

    if (useLightShaft)
    {
        ENGINE_PROFILE_SCOPE_DETAIL("Render::LightShaft");

        const ViewportSize half = { max(1u, viewportWidth / 2u), max(1u, viewportHeight / 2u) };

        RETURN_LOG_IF_FAILED(
            _postprocessShader->Bind_CBufferData(lightShaftParams),
            L"Render_PostProcess:FAIL Bind_CBufferData(LightShaft)");

        RETURN_LOG_IF_FAILED(
            Bind_ScreenSpaceCB(_postprocessShader, half.width, half.height),
            L"Render_PostProcess:FAIL Bind_ScreenSpaceCB(LightShaftMask)");

        // 어디서 god ray가 시작될 수 있는지 흑백 마스크로 만든다.
        RETURN_LOG_IF_FAILED(
            Render_LightShaftPass(
                MRT::LightShaftMaskHalf,    // 출력 
                "g_DepthTexture",
                RT::Depth,                  // 입력
                PostProcessPass::LightShaftMaskHalf),
            L"Render_PostProcess:FAIL Render_LightShaftPass(Mask)");

        RETURN_LOG_IF_FAILED(
            Bind_ScreenSpaceCB(_postprocessShader, half.width, half.height),
            L"Render_PostProcess:FAIL Bind_ScreenSpaceCB(LightShaftBlur)");

        // 마스크를 광원 중심 방향 기준으로 길게 끌어서 빛줄기처럼 만든다
        RETURN_LOG_IF_FAILED(
            Render_LightShaftPass(
                MRT::LightShaftBlurHalf,    // 출력
                "g_LightShaftMaskTexture",
                RT::LightShaftMaskHalf,     // 입력 : 방금 만든 LightShaftMaskHalf
                PostProcessPass::LightShaftRadialBlurHalf),
            L"Render_PostProcess:FAIL Render_LightShaftPass(Blur)");

        RETURN_LOG_IF_FAILED(
            GAME->Copy_RTV(RT::HDRColor, RT::DistortionSource),
            L"Render_PostProcess:FAIL Copy_RTV(HDRColor -> DistortionSource:LightShaft)");

        RETURN_LOG_IF_FAILED(
            Bind_ScreenSpaceCB(_postprocessShader, viewportWidth, viewportHeight),
            L"Render_PostProcess:FAIL Bind_ScreenSpaceCB(LightShaftComposite)");

        RETURN_LOG_IF_FAILED(
            _postprocessShader->Bind_CBufferData(lightShaftParams),
            L"Render_PostProcess:FAIL Bind_CBufferData(LightShaftComposite)");

        RETURN_LOG_IF_FAILED(
            Render_LightShaftCompositePass(),
            L"Render_PostProcess:FAIL Render_LightShaftCompositePass");
    }

    if (_lightingPipeline && _lightingPipeline->Has_FroxelFrameData() && _lightingPipeline->Get_FroxelIntegratedSRV())
    {
        ENGINE_PROFILE_SCOPE_DETAIL("Render::FroxelComposite");

        RETURN_LOG_IF_FAILED(
            GAME->Copy_RTV(RT::HDRColor, RT::DistortionSource),
            L"Render_PostProcess:FAIL Copy_RTV(HDRColor -> DistortionSource:Froxel)");

        RETURN_LOG_IF_FAILED(
            Bind_ScreenSpaceCB(_postprocessShader, viewportWidth, viewportHeight),
            L"Render_PostProcess:FAIL Bind_ScreenSpaceCB(FroxelComposite)");

        RETURN_LOG_IF_FAILED(
            Render_FroxelCompositePass(),
            L"Render_PostProcess:FAIL Render_FroxelCompositePass");
    }

    const bool useDOF = _ppPipeline->Has_PostProcessFeature(PostProcessFeature::DOF);
    if (useDOF)
    {
        ENGINE_PROFILE_SCOPE_DETAIL("Render::DOF");

        const ViewportSize half = { max(1u, viewportWidth / 2u), max(1u, viewportHeight / 2u) };

        RETURN_LOG_IF_FAILED(
            _postprocessShader->Bind_CBufferData(_ppPipeline->Get_PostProcessParams()),
            L"Render_PostProcess:FAIL Bind_CBufferData(DOF)");

        RETURN_LOG_IF_FAILED(
            Bind_ScreenSpaceCB(_postprocessShader, half.width, half.height),
            L"Render_PostProcess:FAIL Bind_ScreenSpaceCB(DOFPreFilter)");

        RETURN_LOG_IF_FAILED(
            Render_DOFPreFilterPass(),
            L"Render_PostProcess:FAIL Render_DOFPreFilterPass");

        RETURN_LOG_IF_FAILED(
            Bind_ScreenSpaceCB(_postprocessShader, half.width, half.height),
            L"Render_PostProcess:FAIL Bind_ScreenSpaceCB(DOFBlurNear)");

        RETURN_LOG_IF_FAILED(
            Render_DOFBlurPass(MRT::DOFNearHalfB, "g_DOFSourceTexture", RT::DOFNearHalfA, PostProcessPass::DOFBlurNearHalf),
            L"Render_PostProcess:FAIL Render_DOFBlurPass(Near)");

        RETURN_LOG_IF_FAILED(
            Bind_ScreenSpaceCB(_postprocessShader, half.width, half.height),
            L"Render_PostProcess:FAIL Bind_ScreenSpaceCB(DOFBlurFar)");

        RETURN_LOG_IF_FAILED(
            Render_DOFBlurPass(MRT::DOFFarHalfB, "g_DOFSourceTexture", RT::DOFFarHalfA, PostProcessPass::DOFBlurFarHalf),
            L"Render_PostProcess:FAIL Render_DOFBlurPass(Far)");

        RETURN_LOG_IF_FAILED(
            GAME->Copy_RTV(RT::HDRColor, RT::DistortionSource),
            L"Render_PostProcess:FAIL Copy_RTV(HDRColor -> DistortionSource:DOF)");

        RETURN_LOG_IF_FAILED(
            Bind_ScreenSpaceCB(_postprocessShader, viewportWidth, viewportHeight),
            L"Render_PostProcess:FAIL Bind_ScreenSpaceCB(DOFComposite)");

        RETURN_LOG_IF_FAILED(
            Render_DOFCompositePass(),
            L"Render_PostProcess:FAIL Render_DOFCompositePass");
    }

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
        L"../../../Client/Bin/Resources/Textures/Shader/IBL/irradiance_eye.dds",
        1);

    _iblPrefilterTexture = Texture::Create(
        _device,
        _context,
        L"../../../Client/Bin/Resources/Textures/Shader/IBL/perfilter_eye.dds",
        1);

    _iblBrdfLutTexture = Texture::Create(
        _device,
        _context,
        L"../../../Client/Bin/Resources/Textures/Shader/IBL/BRDF_LUT.dds",
        1);

    Sync_ViewportDependentState(_screenSize.width, _screenSize.height);

    return S_OK;
}

HRESULT Renderer::Reload_RendererShaders()
{
    uint32 reloadSuccessCount = 0;

    if (Shared<ShaderCom> deferredShader = ShaderCom::Create(
        _device,
        _context,
        L"../../../Client/Bin/ShaderFiles/Shader_Deferred.hlsl",
        VTXTEX::Elements,
        VTXTEX::numElements,
        ShaderType::Deferred))
    {
        _deferredShader = std::move(deferredShader);
        ++reloadSuccessCount;
    }
    else
    {
        LOG_WARN("Renderer shader reload failed: Shader_Deferred.hlsl");
    }

    if (Shared<ShaderCom> postprocessShader = ShaderCom::Create(
        _device,
        _context,
        L"../../../Client/Bin/ShaderFiles/Shader_PostProcess.hlsl",
        VTXTEX::Elements,
        VTXTEX::numElements,
        ShaderType::PostProcess))
    {
        _postprocessShader = std::move(postprocessShader);
        ++reloadSuccessCount;
    }
    else
    {
        LOG_WARN("Renderer shader reload failed: Shader_PostProcess.hlsl");
    }

    if (Shared<ShaderCom> screenFxShader = ShaderCom::Create(
        _device,
        _context,
        L"../../../Client/Bin/ShaderFiles/Shader_ScreenFx.hlsl",
        VTXTEX::Elements,
        VTXTEX::numElements,
        ShaderType::PostProcess))
    {
        _screenFxShader = std::move(screenFxShader);
        ++reloadSuccessCount;
    }
    else
    {
        LOG_WARN("Renderer shader reload failed: Shader_ScreenFx.hlsl");
    }

    if (reloadSuccessCount == 3u)
        return S_OK;

    if (reloadSuccessCount > 0u)
        return S_FALSE;

    return E_FAIL;
}

HRESULT Renderer::Reload_LightingPipelineShaders()
{
    CHECK_NULL(_lightingPipeline, E_FAIL);
    return _lightingPipeline->Reload_FroxelComputeShader();
}

HRESULT Renderer::Begin_DOFOverrideScope(const string& ownerTag)
{
    CHECK_NULL(_ppPipeline, E_FAIL);
    return _ppPipeline->Begin_DOFOverrideScope(ownerTag);
}

HRESULT Renderer::Set_DOFOverridePreset(const string& ownerTag, const DOFPreset& preset, const DOFTransitionDesc& transition)
{
    CHECK_NULL(_ppPipeline, E_FAIL);
    return _ppPipeline->Set_DOFOverridePreset(ownerTag, preset, transition);
}

HRESULT Renderer::Set_DOFOverrideFocus(const string& ownerTag, float focusDistance, float focusRange)
{
    CHECK_NULL(_ppPipeline, E_FAIL);
    return _ppPipeline->Set_DOFOverrideFocus(ownerTag, focusDistance, focusRange);
}

HRESULT Renderer::Clear_DOFOverrideToBaseline(const string& ownerTag, const DOFTransitionDesc& transition)
{
    CHECK_NULL(_ppPipeline, E_FAIL);
    return _ppPipeline->Clear_DOFOverrideToBaseline(ownerTag, transition);
}

HRESULT Renderer::End_DOFOverrideScope(const string& ownerTag, const DOFTransitionDesc& transition)
{
    CHECK_NULL(_ppPipeline, E_FAIL);
    return _ppPipeline->End_DOFOverrideScope(ownerTag, transition);
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

    ScreenSpaceCB screenSpaceCB{};
    screenSpaceCB.screenViewMatrix = _PipelineViewMatrix;
    screenSpaceCB.screenProjMatrix = _pipelineProjMatrix;

    screenSpaceCB.viewInverseMatrix = *GAME->Get_Transform_Inverse(D3DTS::View);
    screenSpaceCB.projInverseMatrix = *GAME->Get_Transform_Inverse(D3DTS::Proj);

    screenSpaceCB.cameraPosition = *GAME->Get_CamPosition();
    screenSpaceCB.farPlane = activeCamera != nullptr ? activeCamera->Get_Far() : 500.f;

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

HRESULT Renderer::Render_LightShaftPass(const wchar_t* dstMrt, const char* srcResourceName, const wchar_t* srcRt, PostProcessPass pass)
{
    const wchar_t* passName = L"LightShaftUnknown";
    switch (pass)
    {
    case PostProcessPass::LightShaftMaskHalf:       passName = L"LightShaftMaskHalf"; break;
    case PostProcessPass::LightShaftRadialBlurHalf: passName = L"LightShaftRadialBlurHalf"; break;
    default: break;
    }

    const wstring beginLog = wstring(L"Render_PostProcess:FAIL Begin_MRT(") + dstMrt + L"," + passName + L")";
    const wstring bindLog = wstring(L"Render_PostProcess:FAIL Bind_RT_ShaderResource(") + std::wstring(srcRt) + L"," + passName + L")";
    const wstring shaderLog = wstring(L"Render_PostProcess:FAIL ShaderBegin(") + passName + L")";
    const wstring renderLog = wstring(L"Render_PostProcess:FAIL RenderRect(") + passName + L")";
    const wstring endLog = wstring(L"Render_PostProcess:FAIL End_MRT(") + dstMrt + L"," + passName + L")";

    RETURN_FAIL_LOG_IF_FAILED(
        GAME->Begin_MRT(dstMrt, NotUseDSV, ClearRTV),
        beginLog.c_str());

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_postprocessShader, srcResourceName, srcRt),
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

HRESULT Renderer::Render_LightShaftCompositePass()
{
    RETURN_FAIL_LOG_IF_FAILED(
        GAME->Begin_MRT(MRT::HDR, NotUseDSV, NotClearRTV),
        L"Render_PostProcess:FAIL Begin_MRT(HDR,LightShaftComposite)");

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_postprocessShader, "g_DistortionSourceTexture", RT::DistortionSource),
        L"Render_PostProcess:FAIL Bind_RT_ShaderResource(g_DistortionSourceTexture-LightShaft)");

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_postprocessShader, "g_LightShaftSourceTexture", RT::LightShaftBlurHalf),
        L"Render_PostProcess:FAIL Bind_RT_ShaderResource(g_LightShaftSourceTexture)");

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        _postprocessShader->Begin(ETOI(PostProcessPass::LightShaftCompositeHDR)),
        L"Render_PostProcess:FAIL ShaderBegin(LightShaftCompositeHDR)");

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        _viBuffer->Render(),
        L"Render_PostProcess:FAIL RenderRect(LightShaftCompositeHDR)");

    RETURN_FAIL_LOG_IF_FAILED(
        GAME->End_MRT(),
        L"Render_PostProcess:FAIL End_MRT(HDR,LightShaftComposite)");

    Add_RenderCall();

    return S_OK;
}

HRESULT Renderer::Render_DOFPreFilterPass()
{
    RETURN_FAIL_LOG_IF_FAILED(
        GAME->Begin_MRT(MRT::DOFPreFilterHalf, NotUseDSV, ClearRTV),
        L"Render_PostProcess:FAIL Begin_MRT(DOFPreFilterHalf)");

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_postprocessShader, "g_HDRTexture", RT::HDRColor),
        L"Render_PostProcess:FAIL Bind_RT_ShaderResource(g_HDRTexture-DOFPreFilter)");

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_postprocessShader, "g_DepthTexture", RT::Depth),
        L"Render_PostProcess:FAIL Bind_RT_ShaderResource(g_DepthTexture-DOFPreFilter)");

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        _postprocessShader->Begin(ETOI(PostProcessPass::DOFPreFilterHalf)),
        L"Render_PostProcess:FAIL ShaderBegin(DOFPreFilterHalf)");

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        _viBuffer->Render(),
        L"Render_PostProcess:FAIL RenderRect(DOFPreFilterHalf)");

    RETURN_FAIL_LOG_IF_FAILED(
        GAME->End_MRT(),
        L"Render_PostProcess:FAIL End_MRT(DOFPreFilterHalf)");

    Add_RenderCall();

    return S_OK;
}

HRESULT Renderer::Render_DOFBlurPass(const wchar_t* dstMrt, const char* srcResourceName, const wchar_t* srcRt, PostProcessPass pass)
{
    const wchar_t* passName = L"DOFUnknown";
    switch (pass)
    {
    case PostProcessPass::DOFBlurNearHalf: passName = L"DOFBlurNearHalf"; break;
    case PostProcessPass::DOFBlurFarHalf:  passName = L"DOFBlurFarHalf"; break;
    default: break;
    }

    const wstring beginLog = wstring(L"Render_PostProcess:FAIL Begin_MRT(") + dstMrt + L"," + passName + L")";
    const wstring srcBindLog = wstring(L"Render_PostProcess:FAIL Bind_RT_ShaderResource(") + std::wstring(srcRt) + L"," + passName + L")";
    const wstring cocBindLog = wstring(L"Render_PostProcess:FAIL Bind_RT_ShaderResource(DOFCoCHalf,") + passName + L")";
    const wstring depthBindLog = wstring(L"Render_PostProcess:FAIL Bind_RT_ShaderResource(Depth,") + passName + L")";
    const wstring shaderLog = wstring(L"Render_PostProcess:FAIL ShaderBegin(") + passName + L")";
    const wstring renderLog = wstring(L"Render_PostProcess:FAIL RenderRect(") + passName + L")";
    const wstring endLog = wstring(L"Render_PostProcess:FAIL End_MRT(") + dstMrt + L"," + passName + L")";

    RETURN_FAIL_LOG_IF_FAILED(
        GAME->Begin_MRT(dstMrt, NotUseDSV, ClearRTV),
        beginLog.c_str());

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_postprocessShader, srcResourceName, srcRt),
        srcBindLog.c_str());

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_postprocessShader, "g_DOFCoCTexture", RT::DOFCoCHalf),
        cocBindLog.c_str());

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_postprocessShader, "g_DepthTexture", RT::Depth),
        depthBindLog.c_str());

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

HRESULT Renderer::Render_DOFCompositePass()
{
    RETURN_FAIL_LOG_IF_FAILED(
        GAME->Begin_MRT(MRT::HDR, NotUseDSV, NotClearRTV),
        L"Render_PostProcess:FAIL Begin_MRT(HDR,DOFComposite)");

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_postprocessShader, "g_DistortionSourceTexture", RT::DistortionSource),
        L"Render_PostProcess:FAIL Bind_RT_ShaderResource(g_DistortionSourceTexture-DOF)");

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_postprocessShader, "g_DOFCoCTexture", RT::DOFCoCHalf),
        L"Render_PostProcess:FAIL Bind_RT_ShaderResource(g_DOFCoCTexture-DOF)");

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_postprocessShader, "g_DOFNearTexture", RT::DOFNearHalfB),
        L"Render_PostProcess:FAIL Bind_RT_ShaderResource(g_DOFNearTexture)");

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_postprocessShader, "g_DOFFarTexture", RT::DOFFarHalfB),
        L"Render_PostProcess:FAIL Bind_RT_ShaderResource(g_DOFFarTexture)");

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        _postprocessShader->Begin(ETOI(PostProcessPass::DOFCompositeHDR)),
        L"Render_PostProcess:FAIL ShaderBegin(DOFCompositeHDR)");

    ENDMRT_RETURN_FAIL_LOG_IF_FAILED(
        _viBuffer->Render(),
        L"Render_PostProcess:FAIL RenderRect(DOFCompositeHDR)");

    RETURN_FAIL_LOG_IF_FAILED(
        GAME->End_MRT(),
        L"Render_PostProcess:FAIL End_MRT(HDR,DOFComposite)");

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

HRESULT Renderer::Render_FroxelCompositePass()
{
    RETURN_FAIL_LOG_IF_FAILED(
        GAME->Begin_MRT(MRT::HDR, NotUseDSV, NotClearRTV),
        L"Render_PostProcess:FAIL Begin_MRT(HDR,FroxelComposite)");

    const bool hasFailed =
        FAILED(GAME->Bind_RT_ShaderResource(_postprocessShader, "g_DistortionSourceTexture", RT::DistortionSource)) ||
        FAILED(GAME->Bind_RT_ShaderResource(_postprocessShader, "g_DepthTexture", RT::Depth)) ||
        FAILED(_postprocessShader->Bind_SRV("g_FroxelIntegratedTexture", _lightingPipeline->Get_FroxelIntegratedSRV())) ||
        FAILED(_postprocessShader->Bind_CBufferData(_lightingPipeline->Get_FroxelParams())) ||
        FAILED(_postprocessShader->Bind_CBufferData(_ppPipeline->Get_PostProcessParams())) ||
        FAILED(_postprocessShader->Begin(ETOI(PostProcessPass::FroxelCompositeHDR))) ||
        FAILED(_viBuffer->Render());

    if (hasFailed)
    {
        _postprocessShader->Bind_SRV("g_FroxelIntegratedTexture", nullptr);
        GAME->End_MRT();
        return E_FAIL;
    }

    _postprocessShader->Bind_SRV("g_FroxelIntegratedTexture", nullptr);

    RETURN_FAIL_LOG_IF_FAILED(
        GAME->End_MRT(),
        L"Render_PostProcess:FAIL End_MRT(HDR,FroxelComposite)");

    return S_OK;
}

bool Renderer::Try_BuildLightShaftParams(LightShaftCB& outParams) const
{
    outParams.lightVisible = 0.f;
    outParams.lightFacingCamera = 0.f;

    if (_ppPipeline == nullptr || !_ppPipeline->Has_PostProcessFeature(PostProcessFeature::LightShaft))
        return false;

    if (outParams.intensity <= 0.0001f)
        return false;

    const LIGHT_DESC* lightDesc = GAME->Get_PrimaryShadowLightDesc();
    if (lightDesc == nullptr || lightDesc->type != LightType::Directional)
        return false;

    const Shared<Camera> activeCamera = GAME->Get_ActiveCamera();
    const Matrix* viewMatrix = GAME->Get_Transform(D3DTS::View);
    const Matrix* projMatrix = GAME->Get_Transform(D3DTS::Proj);
    const Vec4* cameraPosition = GAME->Get_CamPosition();
    if (activeCamera == nullptr || viewMatrix == nullptr || projMatrix == nullptr || cameraPosition == nullptr)
        return false;

    // directional light의 방향으로 태양이 화면 어디에 떠 있는지 계산한 뒤, 현재 프레임에서 sun shaft 를 돌려도 되는지 판정

    // 빛이 어느 방향으로 쏘는지
    Vec4 lightDir = XMLoadFloat4(&lightDesc->direction);
    if (XMVectorGetX(XMVector3LengthSq(lightDir)) <= 1e-6f)
        return false;

    lightDir = XMVector3Normalize(lightDir);

    const XMVECTOR camPos = XMLoadFloat4(cameraPosition);
    // 카메라 far의 절반 거리만큼 떨어진 곳에 태양이 있다는 가정
    const float sunDistance = activeCamera->Get_Far() * 0.5f;

    // sunWorldPos = camPos + (-lightDir) * sunDistance 
    const XMVECTOR sunWorldPos = XMVectorMultiplyAdd(XMVectorNegate(lightDir), XMVectorReplicate(sunDistance), camPos);

    const XMMATRIX view = XMLoadFloat4x4(viewMatrix);
    const XMMATRIX proj = XMLoadFloat4x4(projMatrix);

    // 태양의 view pos -> ndc
    const XMVECTOR sunNdc = XMVector3TransformCoord(sunWorldPos, view * proj);

    // 태양의 ndc pos -> uv
    const float lightScreenX = XMVectorGetX(sunNdc) * 0.5f + 0.5f;
    const float lightScreenY = XMVectorGetY(sunNdc) * -0.5f + 0.5f;
    outParams.lightScreenUV = Vec2{ lightScreenX, lightScreenY };
    outParams.lightVisible = 1.f;
    outParams.lightFacingCamera = 1.f;
    return true;
}

// 빛 시점의 렌더 범위를 카메라가 실제로 보는 공간으로 변경한다.
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

    // shadow 카메라 재계산
    GAME->Update_ShadowFromCameraFrustum(updatedDesc, activeCamera, lightDir);
}

void Renderer::Render_DynamicShadow()
{
    ENGINE_PROFILE_SCOPE_DETAIL("Render::DynamicShadow");

    // 카메라가 보는 영역을 기준으로 빛의 shadow frustum을 업데이트한다.
    Update_DynamicShadowCamera();
    ++_shadowFrameIndex;

    const uint32 cascadeCount = max(1u, GAME->Get_ShadowCascadeCount());
    const uint32 originWidth = _screenSize.width;
    const uint32 originHeight = _screenSize.height;

    Change_ViewportDesc(_shadowMapSize.width, _shadowMapSize.height);

    for (uint32 cascadeIndex = 0; cascadeIndex < cascadeCount; ++cascadeIndex)
    {
        if (FAILED(GAME->Set_ActiveShadowCascade(cascadeIndex)))
            continue;

        // cascade에 따라 특정 프레임만 업데이트 하려고 했는데 mismtach 가 심해서 주석 처리.
        //if (!Should_UpdateDynamicShadowCascade(cascadeIndex))
        //    continue;

        if (GAME->Get_ShadowTransformMatrix(D3DTS::View) == nullptr || GAME->Get_ShadowTransformMatrix(D3DTS::Proj) == nullptr)
            continue;
        if (FAILED(GAME->Begin_MRT(MRT::ShadowObjects, UseDSV, ClearRTV, cascadeIndex)))
            continue;

        for (auto& renderObject : _renderGroups[ETOI(RenderGroup::Shadow)])
        {
            Shared<GameObject> gameObject = renderObject.lock();
            // Shadow projection box가 아니라 별도 influence volume으로 caster를 판정한다.
            if (gameObject != nullptr && Should_RenderDynamicShadowObject(gameObject, cascadeIndex))
            {
                gameObject->Render_Shadow();
                Add_RenderCall();
            }
        }

        CHECK_FAILED(GAME->End_MRT());
    }

    _renderGroups[ETOI(RenderGroup::Shadow)].clear();
    GAME->Set_ActiveShadowCascade(0);

    Change_ViewportDesc(originWidth, originHeight);
}

bool Renderer::Should_UpdateDynamicShadowCascade(uint32 cascadeIndex) const
{
    // 초기 1~2 프레임은 모든 Cascade 업데이트. Shadow Map이 완전히 채워져야 해서
    if (_shadowFrameIndex <= 1ull)
        return true;

    // 0~1 Cascade는 매 프레임 업데이트. 가장 가까운 영역이자 가장 눈에 띄는 영역
    if (cascadeIndex < 2u)
        return true;

    // 2 Cascade는 2프레임마다 업데이트
    if (cascadeIndex == 2u)
        return (_shadowFrameIndex % 2ull) == 0ull;
    
    // 4 Cascade는 4프레임마다 업데이트
    return (_shadowFrameIndex % 4ull) == 0ull;
}

bool Renderer::Should_RenderDynamicShadowObject(const Shared<GameObject>& gameObject, uint32 cascadeIndex) const
{
    if (gameObject == nullptr)
        return false;

    if (gameObject->Get_Transform() == nullptr)
        return true;

    BoundingBox objectBounds{};
    if (!gameObject->Try_Get_ShadowWorldBounds(objectBounds))
        return true;

    return GAME->Is_Visible_In_ShadowInfluenceVolume(objectBounds, cascadeIndex);
}

#ifdef _DEBUG
void Renderer::Sync_ShadowDebugTargets()
{
    static constexpr const wchar_t* kShadowDebugTargets[] =
    {
        RT::LightDepthDebug0,
        RT::LightDepthDebug1,
        RT::LightDepthDebug2,
        RT::LightDepthDebug3,
    };

    const uint32 cascadeCount = min<uint32>(GAME->Get_ShadowCascadeCount(), static_cast<uint32>(std::size(kShadowDebugTargets)));
    for (uint32 cascadeIndex = 0; cascadeIndex < cascadeCount; ++cascadeIndex)
        GAME->Copy_RTV_ArraySlice(RT::LightDepth, cascadeIndex, kShadowDebugTargets[cascadeIndex]);
}
#endif

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

    if (_lightingPipeline && _deferredShader && _viBuffer)
    {
        const auto& rtStates = GAME->Get_RTs();
        const auto froxelDebugIter = rtStates.find(RT::FroxelDebug);
        const bool showFroxelDebug = froxelDebugIter != rtStates.end() && froxelDebugIter->second;

        // 3D 텍스쳐인 froxel debug는 별도 처리
        if (showFroxelDebug)
        {
            CHECK_FAILED(GAME->Begin_MRT(MRT::FroxelDebug, NotUseDSV, ClearRTV));

            const FroxelDebugCB froxelDebugCB
            {
                _lightingPipeline->Get_DebugFroxelWidth(),
                _lightingPipeline->Get_DebugFroxelHeight(),
                _lightingPipeline->Get_DebugFroxelDepth(),
                _lightingPipeline->Get_DebugFroxelSlice(),
                static_cast<uint32>(_lightingPipeline->Get_DebugFroxelMode()),
                _lightingPipeline->Get_DebugFroxelMode() == LightingPipeline::FroxelDebugMode::RawInjection ? 48.f : 1.f
            };

            if (FAILED(_deferredShader->Bind_ObjectCB(_pipelineWorldMatrix)) ||
                FAILED(Bind_ScreenSpaceCB(_deferredShader)) ||
                FAILED(_deferredShader->Bind_CBufferData(froxelDebugCB)) ||
                FAILED(_deferredShader->Bind_SRV("g_FroxelTexture", _lightingPipeline->Get_DebugFroxelSRV())) ||
                FAILED(_viBuffer->Bind_Resources()) ||
                FAILED(_deferredShader->Begin(ETOI(DeferredPass::FroxelDebug))) || /* 별도의 디버그 패스 */
                FAILED(_viBuffer->Render()))
            {
                GAME->End_MRT();
                return;
            }

            CHECK_FAILED(GAME->End_MRT());
            CHECK_FAILED(_deferredShader->Bind_SRV("g_FroxelTexture", nullptr));
        }
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
