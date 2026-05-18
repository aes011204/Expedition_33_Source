#include "Renderer.h"
#include "Camera.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "PostProcessPipeline.h"
#include "ShaderCom.h"
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
    CHECK_FAILED(Ready_RenderTargets(), E_FAIL);
    _ppPipeline = PostProcessPipeline::Create(_device, _context);
    CHECK_NULL(_ppPipeline, E_FAIL);

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
    // ===========================================================================================
    // 렌더 패스 추가하고 싶은데 _DEBUG 이런 거 때문에 넘 복잡하다 싶으면 제게 말해주세요~
    // ===========================================================================================
    Change_ViewportDesc(GAME->Get_ViewportWidth(), GAME->Get_ViewportHeight());

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
    DumpCurrentOMState(L"Render_UI:Before");
#endif

    // ========================================= //
    Render_UI();
    // ========================================= //
#ifdef _DEBUG
    DumpCurrentOMState(L"Render_UI:After");
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

void Renderer::Draw_Preview()
{
#ifdef _DEBUG
    Reset_RenderCall();
#endif

    Render_Priority();
    _renderGroups[ETOI(RenderGroup::Shadow)].clear();

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

    Render_Blend();
    _renderGroups[ETOI(RenderGroup::UI)].clear();
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
    Render_StaticShadowOnce();
    Render_DynamicShadow();
}

void Renderer::Render_NonBlend()
{
    // Diffuse + Normal
    CHECK_FAILED(GAME->Begin_MRT(MRT::GameObjects, NotUseDSV, ClearRTV));

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

    if (FAILED(GAME->Begin_MRT(MRT::LightAcc, NotUseDSV, ClearRTV)))
        return;

    // Transform
    ENDMRT_RETURN_IF_FAILED(
        _deferredShader->Bind_ObjectCB(_pipelineWorldMatrix));

    // depth -> World 위치 복원
    ENDMRT_RETURN_IF_FAILED(Bind_TransformCB(_deferredShader));

    // 명암 연산을 위한 SRV
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
#ifdef _DEBUG
    const auto logFail = [this](const wchar_t* stage) { DumpCurrentOMState(stage); };
#endif

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
        Bind_TransformCB(_deferredShader),
        L"Render_CombinedHDR:FAIL Bind_TransformCB");

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

void Renderer::Render_PostProcess()
{
#ifdef _DEBUG
    const auto logFail = [this](const wchar_t* stage)
        {
            DumpCurrentOMState(stage);
        };
#endif

    if (!_postprocessShader || !_viBuffer)
    {
#ifdef _DEBUG
        logFail(L"Render_PostProcess:FAIL MissingShaderOrBuffer");
#endif
        return;
    }

    if (!_ppPipeline || _ppPipeline->Get_PostProcessFeatures() == PostProcessFeature::None)
    {
#ifdef _DEBUG
        logFail(L"Render_PostProcess:FAIL PostProcessFeatureNone");
#endif
        return;
    }

    if (!_ppPipeline->Has_PostProcessFeature(PostProcessFeature::HDR))
    {
#ifdef _DEBUG
        logFail(L"Render_PostProcess:FAIL HDRDisabled");
#endif
        return;
    }

    // Transform
    RETURN_LOG_IF_FAILED(
        _postprocessShader->Bind_ObjectCB(_pipelineWorldMatrix),
        L"Render_PostProcess:FAIL Bind_ObjectCB");

    // PostProcess Params
    auto& params = _ppPipeline->Get_PostProcessParams();
    const Vec2 baseTexelSize = Vec2{ 1.f / GAME->Get_ViewportWidth(), 1.f / GAME->Get_ViewportHeight() };

    params.bloomTexelSize = baseTexelSize;

    RETURN_LOG_IF_FAILED(
        _postprocessShader->Bind_CBufferData(_ppPipeline->Get_PostProcessParams()),
        L"Render_PostProcess:FAIL Bind_CBufferData(Base)");

    // depth -> World 위치 복원 용도
    RETURN_LOG_IF_FAILED(
        Bind_TransformCB(_postprocessShader),
        L"Render_PostProcess:FAIL Bind_TransformCB");

    // IASet...
    RETURN_LOG_IF_FAILED(
        _viBuffer->Bind_Resources(),
        L"Render_PostProcess:FAIL Bind_Resources");

    // 핑퐁 구조
    const bool useBloom = _ppPipeline->Has_PostProcessFeature(PostProcessFeature::Bloom);
    if (useBloom)
    {
        // HDR -> Bloom 추출
        RETURN_LOG_IF_FAILED(
            GAME->Begin_MRT(MRT::BloomExtract, NotUseDSV, ClearRTV),
            L"Render_PostProcess:FAIL Begin_MRT(BloomExtract)");

        ENDMRT_RETURN_LOG_IF_FAILED(
            GAME->Bind_RT_ShaderResource(_postprocessShader, "g_HDRTexture", RT::HDRColor),
            L"Render_PostProcess:FAIL Bind_RT_ShaderResource(g_HDRTexture)");

        ENDMRT_RETURN_LOG_IF_FAILED(
            _postprocessShader->Begin(ETOI(PostProcessPass::BloomExtract)),
            L"Render_PostProcess:FAIL ShaderBegin(BloomExtract)");

        ENDMRT_RETURN_LOG_IF_FAILED(
            _viBuffer->Render(),
            L"Render_PostProcess:FAIL RenderRect(BloomExtract)");

        RETURN_LOG_IF_FAILED(
            GAME->End_MRT(),
            L"Render_PostProcess:FAIL End_MRT(BloomExtract)");

        Add_RenderCall();

        // Bloom Downsample : BloomExtract -> BloomBlur
        RETURN_LOG_IF_FAILED(
            GAME->Begin_MRT(MRT::BloomBlur, NotUseDSV, ClearRTV),
            L"Render_PostProcess:FAIL Begin_MRT(BloomBlur-Downsample)");

        ENDMRT_RETURN_LOG_IF_FAILED(
            GAME->Bind_RT_ShaderResource(_postprocessShader, "g_BloomTexture", RT::BloomExtract),
            L"Render_PostProcess:FAIL Bind_RT_ShaderResource(g_BloomTexture:Extract)");

        ENDMRT_RETURN_LOG_IF_FAILED(
            _postprocessShader->Begin(ETOI(PostProcessPass::BloomDownsample)),
            L"Render_PostProcess:FAIL ShaderBegin(BloomDownsample)");

        ENDMRT_RETURN_LOG_IF_FAILED(
            _viBuffer->Render(),
            L"Render_PostProcess:FAIL RenderRect(BloomDownsample)");

        RETURN_LOG_IF_FAILED(
            GAME->End_MRT(),
            L"Render_PostProcess:FAIL End_MRT(BloomBlur-Downsample)");

        Add_RenderCall();

        // 텍셀 사이즈를 키워서 다운샘플링 효과를 낸다.
        params.bloomTexelSize = Vec2{ baseTexelSize.x * 2.f, baseTexelSize.y * 2.f };

        RETURN_LOG_IF_FAILED(
            _postprocessShader->Bind_CBufferData(_ppPipeline->Get_PostProcessParams()),
            L"Render_PostProcess:FAIL Bind_CBufferData(DownsampleTexel)");

        // Bloom Blur_H : BloomBlur -> BloomExtract
        RETURN_LOG_IF_FAILED(
            GAME->Begin_MRT(MRT::BloomExtract, NotUseDSV, ClearRTV),
            L"Render_PostProcess:FAIL Begin_MRT(BloomExtract-BlurH)");

        ENDMRT_RETURN_LOG_IF_FAILED(
            GAME->Bind_RT_ShaderResource(_postprocessShader, "g_BloomTexture", RT::BloomBlur),
            L"Render_PostProcess:FAIL Bind_RT_ShaderResource(g_BloomTexture:BlurSrc)");

        ENDMRT_RETURN_LOG_IF_FAILED(
            _postprocessShader->Begin(ETOI(PostProcessPass::BloomBlurH)),
            L"Render_PostProcess:FAIL ShaderBegin(BloomBlurH)");

        ENDMRT_RETURN_LOG_IF_FAILED(
            _viBuffer->Render(),
            L"Render_PostProcess:FAIL RenderRect(BloomBlurH)");

        RETURN_LOG_IF_FAILED(
            GAME->End_MRT(),
            L"Render_PostProcess:FAIL End_MRT(BloomExtract-BlurH)");

        Add_RenderCall();

        // Bloom Blur_V : BloomExtract -> BloomBlur
        RETURN_LOG_IF_FAILED(
            GAME->Begin_MRT(MRT::BloomBlur, NotUseDSV, ClearRTV),
            L"Render_PostProcess:FAIL Begin_MRT(BloomBlur-BlurV)");

        ENDMRT_RETURN_LOG_IF_FAILED(
            GAME->Bind_RT_ShaderResource(_postprocessShader, "g_BloomTexture", RT::BloomExtract),
            L"Render_PostProcess:FAIL Bind_RT_ShaderResource(g_BloomTexture:ExtractForV)");

        ENDMRT_RETURN_LOG_IF_FAILED(
            _postprocessShader->Begin(ETOI(PostProcessPass::BloomBlurV)),
            L"Render_PostProcess:FAIL ShaderBegin(BloomBlurV)");

        ENDMRT_RETURN_LOG_IF_FAILED(
            _viBuffer->Render(),
            L"Render_PostProcess:FAIL RenderRect(BloomBlurV)");

        RETURN_LOG_IF_FAILED(
            GAME->End_MRT(),
            L"Render_PostProcess:FAIL End_MRT(BloomBlur-BlurV)");

        Add_RenderCall();
    }

    // Light 기록
    params.bloomTexelSize = baseTexelSize;

    RETURN_LOG_IF_FAILED(
        _postprocessShader->Bind_CBufferData(_ppPipeline->Get_PostProcessParams()),
        L"Render_PostProcess:FAIL Bind_CBufferData(ToneMap)");

    RETURN_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_postprocessShader, "g_HDRTexture", RT::HDRColor),
        L"Render_PostProcess:FAIL Bind_RT_ShaderResource(g_HDRTexture-ToneMap)");

    RETURN_LOG_IF_FAILED(
        GAME->Bind_RT_ShaderResource(_postprocessShader, "g_BloomTexture", RT::BloomBlur),
        L"Render_PostProcess:FAIL Bind_RT_ShaderResource(g_BloomTexture-ToneMap)");

    RETURN_LOG_IF_FAILED(
        _postprocessShader->Begin(ETOI(PostProcessPass::ToneMapping)),
        L"Render_PostProcess:FAIL ShaderBegin(ToneMapping)");

    const uint32 passIndex = _usePreviewDeferredCombine
        ? ETOI(DeferredPass::CombinedPreview)
        : ETOI(DeferredPass::CombinedHDR);       // 일반 runtime/gameplay는 기존 shadow combine 유지

    RETURN_LOG_IF_FAILED(
        _viBuffer->Render(),
        L"Render_PostProcess:FAIL RenderRect(ToneMapping)");

    Add_RenderCall();
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
    CHECK_FAILED(GAME->Add_RenderTarget(RT::Diffuse,
        width, height, DXGI_FORMAT_R8G8B8A8_UNORM, Color(0, 0, 0, 0), resizeWithViewport, !useDSV), E_FAIL);
    CHECK_FAILED(GAME->Add_RenderTarget(RT::Normal,
        width, height, DXGI_FORMAT_R16G16B16A16_UNORM, Color(0, 0, 0, 0), resizeWithViewport, !useDSV), E_FAIL);
    CHECK_FAILED(GAME->Add_RenderTarget(RT::Depth,
        width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, Color(0, 0, 0, 0), resizeWithViewport, !useDSV), E_FAIL);
    CHECK_FAILED(GAME->Add_RenderTarget(RT::Material,
        width, height, DXGI_FORMAT_R8G8B8A8_UNORM, Color(0, 0, 0, 0), resizeWithViewport, !useDSV), E_FAIL);
    CHECK_FAILED(GAME->Add_RenderTarget(RT::Emissive,
        width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, Color(0, 0, 0, 0), resizeWithViewport, !useDSV), E_FAIL);

    // SSAO
    CHECK_FAILED(GAME->Add_RenderTarget(RT::SSAO,
        width, height, DXGI_FORMAT_R16_FLOAT, Color(1.f, 1.f, 1.f, 1.f), resizeWithViewport, !useDSV), E_FAIL);
    CHECK_FAILED(GAME->Add_RenderTarget(RT::SSAOBlur,
        width, height, DXGI_FORMAT_R16_FLOAT, Color(1.f, 1.f, 1.f, 1.f), resizeWithViewport, !useDSV), E_FAIL);

    // MRT_LightAcc
    CHECK_FAILED(GAME->Add_RenderTarget(RT::Shade,
        width, height, DXGI_FORMAT_R16G16B16A16_UNORM, Color(0, 0, 0, 0), resizeWithViewport, !useDSV), E_FAIL);
    CHECK_FAILED(GAME->Add_RenderTarget(RT::Specular,
        width, height, DXGI_FORMAT_R16G16B16A16_UNORM, Color(0, 0, 0, 0), resizeWithViewport, !useDSV), E_FAIL);

    // MRT_ShadowObjects
    CHECK_FAILED(GAME->Add_RenderTarget(RT::LightDepth,
        _shadowMapWidth, _shadowMapHeight,
        DXGI_FORMAT_R32G32B32A32_FLOAT, Color(1.f, 1.f, 1.f, 1.f), !resizeWithViewport, useDSV), E_FAIL);

    CHECK_FAILED(GAME->Add_RenderTarget(RT::StaticLightDepth,
        _staticShadowMapWidth, _staticShadowMapHeight,
        DXGI_FORMAT_R32G32B32A32_FLOAT, Color(1.f, 1.f, 1.f, 1.f), !resizeWithViewport, useDSV), E_FAIL);


    // MRT_PostProcess
    CHECK_FAILED(GAME->Add_RenderTarget(RT::BloomExtract,
        width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, Color(0.f, 0.f, 0.f, 1.f), resizeWithViewport, !useDSV), E_FAIL);
    CHECK_FAILED(GAME->Add_RenderTarget(RT::BloomBlur,
        width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, Color(0.f, 0.f, 0.f, 1.f), resizeWithViewport, !useDSV), E_FAIL);

    CHECK_FAILED(GAME->Add_RenderTarget(RT::HDRColor,
        width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, Color(0.f, 0.f, 0.f, 1.f), resizeWithViewport, !useDSV), E_FAIL);

    //CHECK_FAILED(Ready_ShadowDSV(), E_FAIL);

    // Create MRT
    CHECK_FAILED(GAME->Add_MRT(MRT::GameObjects, RT::Diffuse), E_FAIL);		// SV_TARGET0
    CHECK_FAILED(GAME->Add_MRT(MRT::GameObjects, RT::Normal), E_FAIL);		// SV_TARGET1
    CHECK_FAILED(GAME->Add_MRT(MRT::GameObjects, RT::Depth), E_FAIL);		// SV_TARGET2
    CHECK_FAILED(GAME->Add_MRT(MRT::GameObjects, RT::Material), E_FAIL);	// SV_TARGET3
    CHECK_FAILED(GAME->Add_MRT(MRT::GameObjects, RT::Emissive), E_FAIL);	// SV_TARGET4

    // SSAO


    // Light
    CHECK_FAILED(GAME->Add_MRT(MRT::LightAcc, RT::Shade), E_FAIL);			// SV_TARGET0
    CHECK_FAILED(GAME->Add_MRT(MRT::LightAcc, RT::Specular), E_FAIL);		// SV_TARGET1

    CHECK_FAILED(GAME->Add_MRT(MRT::ShadowObjects, RT::LightDepth), E_FAIL);
    CHECK_FAILED(GAME->Add_MRT(MRT::StaticShadowObjects, RT::StaticLightDepth), E_FAIL);

    // PostProcess
    CHECK_FAILED(GAME->Add_MRT(MRT::HDR, RT::HDRColor), E_FAIL);

    CHECK_FAILED(GAME->Add_MRT(MRT::BloomExtract, RT::BloomExtract), E_FAIL);

    CHECK_FAILED(GAME->Add_MRT(MRT::BloomBlur, RT::BloomBlur), E_FAIL);

    // deferred shader, rect buffer 생성
    _viBuffer = VIBuffer_Rect::Create(_device, _context);
    CHECK_NULL(_viBuffer, E_FAIL);

    _deferredShader = ShaderCom::Create(_device, _context,
        L"../../../Client/Bin/ShaderFiles/Shader_Deferred.hlsl", VTXTEX::Elements, VTXTEX::numElements, ShaderType::Deferred);
    CHECK_NULL(_deferredShader, E_FAIL);

    _postprocessShader = ShaderCom::Create(_device, _context,
        L"../../../Client/Bin/ShaderFiles/Shader_PostProcess.hlsl", VTXTEX::Elements, VTXTEX::numElements, ShaderType::PostProcess);
    CHECK_NULL(_postprocessShader, E_FAIL);

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

HRESULT Renderer::Bind_TransformCB(Shared<ShaderCom> shader) const
{
    CHECK_NULL(shader, E_FAIL);

    Shared<Camera> activeCamera = GAME->Get_ActiveCamera();
    if (activeCamera == nullptr)
        return E_FAIL;

    DeferredCB deferredCB{};
    deferredCB.screenViewMatrix = _PipelineViewMatrix;
    deferredCB.screenProjMatrix = _pipelineProjMatrix;

    deferredCB.viewInverseMatrix = *GAME->Get_Transform_Inverse(D3DTS::View);
    deferredCB.projInverseMatrix = *GAME->Get_Transform_Inverse(D3DTS::Proj);

    deferredCB.cameraPosition = *GAME->Get_CamPosition();
    deferredCB.farPlane = activeCamera->Get_Far();

    return shader->Bind_CBufferData(deferredCB);
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
    updatedDesc.shadowMapWidth = _shadowMapWidth;
    updatedDesc.shadowMapHeight = _shadowMapHeight;

    const Vec3 lightDir = Vec3(
        lightDesc->direction.x,
        lightDesc->direction.y,
        lightDesc->direction.z
    );

    GAME->Update_ShadowFromCameraFrustum(updatedDesc, activeCamera, lightDir);
}

void Renderer::Render_StaticShadowOnce()
{
    Debug_Profile::Scope profileScope("Render::StaticShadow");

    if (!_staticShadowDirty)
        return;

    BoundingBox staticBounds{};
    if (!Build_StaticShadowBounds(staticBounds))
        return;

    Update_StaticShadowCamera(staticBounds);

    if (FAILED(GAME->Begin_MRT(MRT::StaticShadowObjects, UseDSV, ClearRTV)))
        return;

    const uint32 originWidth = GAME->Get_ViewportWidth();
    const uint32 originHeight = GAME->Get_ViewportHeight();

    Change_ViewportDesc(_staticShadowMapWidth, _staticShadowMapHeight);

    for (const auto& weakObject : _staticShadowCasters)
    {
        Shared<GameObject> gameObject = weakObject.lock();
        if (gameObject == nullptr || !gameObject->Casts_StaticShadow())
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
    Debug_Profile::Scope profileScope("Render::DynamicShadow");

    Update_DynamicShadowCamera();

    if (FAILED(GAME->Begin_MRT(MRT::ShadowObjects, UseDSV, ClearRTV)))
        return;

    const uint32 originWidth = GAME->Get_ViewportWidth();
    const uint32 originHeight = GAME->Get_ViewportHeight();

    Change_ViewportDesc(_shadowMapWidth, _shadowMapHeight);

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

void Renderer::DumpCurrentOMState(const wchar_t* stageTag) const
{
    return; // 컴파일러가 무시할 거임
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

    outSummary.combinedHDRAvgMs = static_cast<float>(
        Safe_Divide(_renderPerfWindow.combinedHDR.accumulatedMs, static_cast<double>(_renderPerfWindow.combinedHDR.sampleCount)));
    outSummary.combinedHDRMaxMs = static_cast<float>(_renderPerfWindow.combinedHDR.maxMs);

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
    const double combinedHDRMs,
    const double postprocessMs,
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
    Accumulate_PassPerf(_renderPerfWindow.combinedHDR, combinedHDRMs);
    Accumulate_PassPerf(_renderPerfWindow.postProcess, postprocessMs);
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

    // 디버그 RT 프리뷰는 Deferred 셰이더의 VS_MAIN(= g_ViewMatrix/g_ProjMatrix 사용)로 그린다.
    // 다른 패스/리사이즈로 값이 바뀌면 화면 밖으로 날아갈 수 있으니 여기서 매번 명시적으로 바인딩한다.
    if (_deferredShader)
    {
        const uint32 width = GAME->Get_ViewportWidth();
        const uint32 height = GAME->Get_ViewportHeight();

        _PipelineViewMatrix = Matrix::Identity;
        _pipelineProjMatrix = XMMatrixOrthographicLH(static_cast<float>(width), static_cast<float>(height), 0.f, 1.f);

        CHECK_FAILED(Bind_TransformCB(_deferredShader));
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
