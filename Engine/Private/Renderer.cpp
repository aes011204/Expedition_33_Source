#include "Renderer.h"

#include "GameInstance.h"
#include "GameObject.h"
#include "ShaderCom.h"
#include "VIBuffer_Rect.h"

/*
 * TODO . 렌더 타겟 생성 및 MRT 구성
 * TODO . Deferred Shader 생성
 * TODO . 그림자 및 빛 연산
 * TODO . Combined
 * TODO . 이외 후처리
 */

Renderer::Renderer(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : _device { device }
	, _context { context }
{
}

HRESULT Renderer::Initialize()
{
    CHECK_FAILED(Ready_RenderTargets(), E_FAIL);

	return S_OK;
}

void Renderer::Add_RenderGroup(RenderGroup renderGroup, Shared<GameObject> gameObject)
{
    CHECK_NULL(gameObject);

    if (renderGroup < RenderGroup::Priority || renderGroup >= RenderGroup::END)
        return;

    _renderGroups[ETOI(renderGroup)].push_back(gameObject);
}

void Renderer::Draw()
{
    Render_Priority();

    Render_Shadow();

    Render_NonBlend();

    Render_Lights();

    Render_Combined();

    Render_NonLight();

    Render_Blend();

    Render_UI();

#ifdef _DEBUG
    // Render_Debug();
#endif
}

void Renderer::Render_Priority()
{
	for (auto& renderObject : _renderGroups[ETOI(RenderGroup::Priority)])
	{
		Shared<GameObject> gameObject = renderObject.lock();
		if (nullptr != gameObject)
			gameObject->Render();
	}

	_renderGroups[ETOI(RenderGroup::Priority)].clear();
}

void Renderer::Render_Shadow()
{
    if (FAILED(GAME->Begin_MRT(L"MRT_ShadowObjects", _shadowDepthDSV)))
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
            gameObject->Render();
    }

    _renderGroups[ETOI(RenderGroup::Shadow)].clear();

    CHECK_FAILED(GAME->End_MRT());

    Change_ViewportDesc(originWidth, originHeight);
}

void Renderer::Render_NonBlend()
{
    // Diffuse + Normal
    CHECK_FAILED(GAME->Begin_MRT(L"MRT_GameObjects"));

    // TODO 필요 시 정렬
	for (auto& renderObject : _renderGroups[ETOI(RenderGroup::NonBlend)])
	{
		Shared<GameObject> gameObject = renderObject.lock();
		if (nullptr != gameObject)
			gameObject->Render();
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

    if (FAILED(GAME->End_MRT()))
        return;
}

void Renderer::Render_NonLight()
{
    for (auto& renderObject : _renderGroups[ETOI(RenderGroup::NonLight)])
    {
        Shared<GameObject> gameObject = renderObject.lock();
        if (nullptr != gameObject)
            gameObject->Render();
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

    // 그림자
    if (FAILED(GAME->Bind_ShadowMatrices(_deferredShader,  "g_LightViewMatrix", "g_LightProjMatrix")))
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
}


void Renderer::Render_Blend()
{
	for (auto& renderObject : _renderGroups[ETOI(RenderGroup::Blend)])
	{
		Shared<GameObject> gameObject = renderObject.lock();
		if (nullptr != gameObject)
			gameObject->Render();
	}

	_renderGroups[ETOI(RenderGroup::Blend)].clear();
}

void Renderer::Render_UI()
{
	for (auto& renderObject : _renderGroups[ETOI(RenderGroup::UI)])
	{
		Shared<GameObject> gameObject = renderObject.lock();
		if (nullptr != gameObject)
			gameObject->Render();
	}

	_renderGroups[ETOI(RenderGroup::UI)].clear();
}

HRESULT Renderer::Ready_RenderTargets()
{
    const uint32 width = GAME->Get_ViewportWidth();
    const uint32 height = GAME->Get_ViewportHeight();

    // MRT_GameObjects
    CHECK_FAILED(GAME->Add_RenderTarget(L"Target_Diffuse",
        width, height, DXGI_FORMAT_R8G8B8A8_UNORM, Color(0, 0, 0, 0)), E_FAIL);

    CHECK_FAILED(GAME->Add_RenderTarget(L"Target_Normal",
        width, height, DXGI_FORMAT_R16G16B16A16_UNORM, Color(0, 0, 0, 0)), E_FAIL);

    CHECK_FAILED(GAME->Add_RenderTarget(L"Target_Depth",
        width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, Color(0, 0, 0, 0)), E_FAIL);

    // MRT_LightAcc
    CHECK_FAILED(GAME->Add_RenderTarget(L"Target_Shade",
        width, height, DXGI_FORMAT_R16G16B16A16_UNORM, Color(0, 0, 0, 0)), E_FAIL);

    CHECK_FAILED(GAME->Add_RenderTarget(L"Target_Specular",
        width, height, DXGI_FORMAT_R16G16B16A16_UNORM, Color(0, 0, 0, 0)), E_FAIL);

    // MRT_ShadowObjects
    CHECK_FAILED(GAME->Add_RenderTarget(L"Target_LightDepth",
        width * static_cast<uint32>(_shadowScale), height * static_cast<uint32>(_shadowScale), DXGI_FORMAT_R32G32B32A32_FLOAT, Color(1.f, 1.f, 1.f, 1.f), false), E_FAIL);

    CHECK_FAILED(Ready_ShadowDSV(), E_FAIL);

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
        L"../../../Client/Bin/ShaderFiles/Shader_Deferred.hlsl", VTXTEX::Elements, VTXTEX::numElements);
    CHECK_NULL(_deferredShader, E_FAIL);

    _deferredWorldMatrix = Matrix::CreateScale(static_cast<float>(width), static_cast<float>(height), 1.f);
    _deferredViewMatrix = Matrix::Identity;
    _deferredProjMatrix = XMMatrixOrthographicLH(static_cast<float>(width), static_cast<float>(height), 0.f, 1.f);

#ifdef _DEBUG
    // 전체 뷰포트의 1/4만 사용하여 디버그 렌더
    GAME->Initialize_Debug(width * 0.5f, height * 0.5f);
#endif

    return S_OK;
}

HRESULT Renderer::Ready_ShadowDSV()
{
    const float shadowWidth = static_cast<float>(GAME->Get_ViewportWidth()) * _shadowScale;
    const float shadowHeight = static_cast<float>(GAME->Get_ViewportHeight()) * _shadowScale;

    _shadowViewport.TopLeftX = 0.f;
    _shadowViewport.TopLeftY = 0.f;
    _shadowViewport.Width = shadowWidth;
    _shadowViewport.Height = shadowHeight;
    _shadowViewport.MinDepth = 0.f;
    _shadowViewport.MaxDepth = 1.f;

    _shadowDepthTexture.Reset();
    _shadowDepthDSV.Reset();

    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = static_cast<uint32>(shadowWidth);
    depthDesc.Height = static_cast<uint32>(shadowHeight);
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    CHECK_FAILED(_device->CreateTexture2D(&depthDesc, nullptr, _shadowDepthTexture.GetAddressOf()), E_FAIL);
    CHECK_FAILED(_device->CreateDepthStencilView(_shadowDepthTexture.Get(), nullptr, _shadowDepthDSV.GetAddressOf()), E_FAIL);

    return S_OK;
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

    _debugComponents.clear();

    CHECK_FAILED(GAME->Render_RT_Debug(_viBuffer, _deferredShader));
}

#endif

#ifdef ENDMRT_RETURN_IF_FAILED
#undef ENDMRT_RETURN_IF_FAILED
#endif
