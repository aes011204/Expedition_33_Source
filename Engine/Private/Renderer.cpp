#include "Renderer.h"

#include "GameInstance.h"
#include "GameObject.h"

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

	Render_NonBlend();

	Render_Blend();

	Render_UI();
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

void Renderer::Render_NonBlend()
{
	for (auto& renderObject : _renderGroups[ETOI(RenderGroup::NonBlend)])
	{
		Shared<GameObject> gameObject = renderObject.lock();
		if (nullptr != gameObject)
			gameObject->Render();
	}

	_renderGroups[ETOI(RenderGroup::NonBlend)].clear();
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
    const uint32 width = static_cast<uint32>(GAME->Get_ViewportWidth());
    const uint32 height = static_cast<uint32>(GAME->Get_ViewportHeight());

    CHECK_FAILED(GAME->Add_RenderTarget(L"Target_Diffuse",
        width, height, DXGI_FORMAT_R8G8B8A8_UNORM, Color(0, 0, 0, 0)), E_FAIL);

    CHECK_FAILED(GAME->Add_RenderTarget(L"Target_Normal",
        width, height, DXGI_FORMAT_R16G16B16A16_UNORM, Color(0, 0, 0, 0)), E_FAIL);

    CHECK_FAILED(GAME->Add_RenderTarget(L"Target_Shade",
        width, height, DXGI_FORMAT_R16G16B16A16_UNORM, Color(0, 0, 0, 0)), E_FAIL);

    CHECK_FAILED(GAME->Add_RenderTarget(L"Target_Depth",
        width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, Color(0, 0, 0, 0)), E_FAIL);

    CHECK_FAILED(GAME->Add_RenderTarget(L"Target_Specular",
        width, height, DXGI_FORMAT_R16G16B16A16_UNORM, Color(0, 0, 0, 0)), E_FAIL);

    // shadow
    CHECK_FAILED(GAME->Add_RenderTarget(L"Target_LightDepth",
        width * static_cast<uint32>(_shadowScale), height * static_cast<uint32>(_shadowScale), DXGI_FORMAT_R32G32B32A32_FLOAT, Color(1.f, 1.f, 1.f, 1.f), false), E_FAIL);

    CHECK_FAILED(GAME->Add_MRT(L"MRT_GameObjects", L"Target_Diffuse"), E_FAIL); // SV_TARGET0
    CHECK_FAILED(GAME->Add_MRT(L"MRT_GameObjects", L"Target_Normal"), E_FAIL);  // SV_TARGET1
    CHECK_FAILED(GAME->Add_MRT(L"MRT_GameObjects", L"Target_Depth"), E_FAIL);   // SV_TARGET2

    CHECK_FAILED(GAME->Add_MRT(L"MRT_LightAcc", L"Target_Shade"), E_FAIL);      // SV_TARGET0
    CHECK_FAILED(GAME->Add_MRT(L"MRT_LightAcc", L"Target_Specular"), E_FAIL);   // SV_TARGET1

    CHECK_FAILED(GAME->Add_MRT(L"MRT_ShadowObjects", L"Target_LightDepth"), E_FAIL);


    _deferredShader = Shader::Create(_device, _context,
        L"../../Client/Bin/ShaderFiles/Shader_Deferred.hlsl", VTXTEX::Elements, VTXTEX::numElements);

    _deferredWorldMatrix = Matrix::CreateScale(static_cast<float>(width), static_cast<float>(height), 1.f);
    _deferredViewMatrix = Matrix::Identity;
    _deferredProjMatrix = XMMatrixOrthographicLH(static_cast<float>(width), static_cast<float>(height), 0.f, 1.f);

    // TODO : Debug

    return S_OK;
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

