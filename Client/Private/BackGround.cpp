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

HRESULT BackGround::Initialize(void* pArg)
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

void BackGround::Priority_Update(float fTimeDelta)
{
	int a = 10;
}

void BackGround::Update(float fTimeDelta)
{
	 
	_positionX += 10.f * fTimeDelta;

	__super::Update_Transform();
}

void BackGround::Late_Update(float fTimeDelta)
{
 	GAME->Add_RenderGroup(RenderGroup::UI, this);
}

HRESULT BackGround::Render()
{
	if (FAILED(_transformCom->Bind_ShaderResource(_shader, "g_WorldMatrix")))
		return E_FAIL;

	if (FAILED(__super::Bind_ShaderResource(_shader, "g_ViewMatrix", D3DTS::VIEW)))
		return E_FAIL;

	if (FAILED(__super::Bind_ShaderResource(_shader, "g_ProjMatrix", D3DTS::PROJ)))
		return E_FAIL;

	if (FAILED(_texture->Bind_ShaderResourceView(_shader, "g_Texture", 0)))
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
	if (FAILED(__super::Add_Component(ETOI(LevelType::Logo), TEXT("Prototype_Component_Texture_BackGround"),
		TEXT("Com_Texture"), reinterpret_cast<Component**>(&_texture))))
		return E_FAIL;

	if (FAILED(__super::Add_Component(ETOI(LevelType::Static), TEXT("Prototype_Component_Shader_VtxTex"),
		TEXT("Com_Shader"), reinterpret_cast<Component**>(&_shader))))
		return E_FAIL;

	if (FAILED(__super::Add_Component(ETOI(LevelType::Static), TEXT("Prototype_Component_VIBuffer_Rect"),
		TEXT("Com_VIBuffer"), reinterpret_cast<Component**>(&_vibuffer))))
		return E_FAIL;







	return S_OK;
}

Shared<BackGround> BackGround::Create(ComPtr<Device> device, ComPtr<Context> context)
{
	Shared<BackGround> pInstance = MakeSharedFromRaw(new BackGround(device, context));

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : BackGround");
		Safe_Release(pInstance);
	}
	return pInstance;
}


Shared<GameObject> BackGround::Clone(void* pArg)
{
	Shared<BackGround> pInstance = MakeSharedFromRaw(new BackGround(*this));

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : BackGround");
		Safe_Release(pInstance);
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



