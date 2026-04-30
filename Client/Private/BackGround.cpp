#include "BackGround.h"
#include "GameInstance.h"

BackGround::BackGround(ComPtr<Device> device, ComPtr<Context> context)
	: UIObject { device, context }
{
}

BackGround::BackGround(const BackGround& Prototype)
	: UIObject { Prototype }

{
}

HRESULT BackGround::Initialize_Prototype()
{
	return S_OK;
}

HRESULT BackGround::Initialize(void* arg)
{
	BACKGROUND_DESC			Desc{};

	Desc.fDegreePerSec = 90.f;
	Desc.fSpeedPerSec = 1.f;
	Desc.fX = 100.f;
	Desc.fY = 100.f;
	Desc.fSizeX = 200.f;
	Desc.fSizeY = 200.f;
	
	if (FAILED(__super::Initialize(&Desc)))
		return E_FAIL;
	
	if (FAILED(Ready_Components()))
		return E_FAIL;

	return S_OK;
}

void BackGround::Priority_Update(float timeDelta)
{
	int a = 10;
}

void BackGround::Update(float timeDelta)
{
	 
	_positionX += 10.f * timeDelta;

	__super::Update_Transform();
}

void BackGround::Late_Update(float timeDelta)
{
 	GAME->Add_RenderGroup(RenderGroup::UI, GetSharedPtr<GameObject>());
}

HRESULT BackGround::Render()
{
	if (FAILED(_transformCom->Bind_ShaderResource(_shader.get(), "g_WorldMatrix")))
		return E_FAIL;

	if (FAILED(__super::Bind_ShaderResource(_shader.get(), "g_ViewMatrix", D3DTS::View)))
		return E_FAIL;

	if (FAILED(__super::Bind_ShaderResource(_shader.get(), "g_ProjMatrix", D3DTS::Proj)))
		return E_FAIL;

	if (FAILED(_texture->Bind_ShaderResourceView(_shader.get(), "g_Texture", 0)))
		return E_FAIL;

	if (FAILED(_shader->Begin(0)))
		return E_FAIL;

	if (FAILED(_vibuffer->Bind_Resources()))
		return E_FAIL;

	if (FAILED(_vibuffer->Render()))
		return E_FAIL;


	return S_OK;
}

HRESULT BackGround::Ready_Components()
{
	if (FAILED(__super::Add_Component(ETOI(LevelType::Logo), TEXT("Texture_BackGround"), _texture)))
		return E_FAIL;

	if (FAILED(__super::Add_Component(ETOI(LevelType::Static), TEXT("Shader_VtxTex"), _shader)))
		return E_FAIL;

	if (FAILED(__super::Add_Component(ETOI(LevelType::Static), TEXT("VIBuffer_Rect"), _vibuffer)))
		return E_FAIL;







	return S_OK;
}

Shared<BackGround> BackGround::Create(ComPtr<Device> device, ComPtr<Context> context)
{
	auto pInstance = make_shared<BackGround>(device, context);

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : BackGround");
		return nullptr;
	}
	return pInstance;
}


Shared<GameObject> BackGround::Clone(void* arg)
{
	auto pInstance = make_shared<BackGround>(*this);

	if (FAILED(pInstance->Initialize(arg)))
	{
		MSG_BOX("Failed to Cloned : BackGround");
		return nullptr;
	}
	return pInstance;
}

void BackGround::Free()
{
	__super::Free();

	
	Safe_Release(_shader);
	Safe_Release(_texture);
	Safe_Release(_vibuffer);
}



