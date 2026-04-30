#include "Sky.h"
#include "GameInstance.h"

Sky::Sky(ComPtr<Device> device, ComPtr<Context> context)
	: GameObject { device, context }
{
}

Sky::Sky(const Sky& Prototype)
	: GameObject { Prototype }

{
}

HRESULT Sky::Initialize_Prototype()
{
	return S_OK;
}

HRESULT Sky::Initialize(void* pArg)
{	
	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;
	
	if (FAILED(Ready_Components()))
		return E_FAIL;

	return S_OK;
}

void Sky::Priority_Update(float fTimeDelta)
{
	int a = 10;
}

void Sky::Update(float fTimeDelta)
{
	_transformCom->Set_State(State::POSITION,
		XMLoadFloat4(GAME->Get_CamPosition()));
}

void Sky::Late_Update(float fTimeDelta)
{
 	GAME->Add_RenderGroup(RenderGroup::PRIORITY, this);
}

HRESULT Sky::Render()
{
	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;

	if (FAILED(_shader->Begin(0)))
		return E_FAIL;

	if (FAILED(_vibuffer->Bind_Resources()))
		return E_FAIL;

	if (FAILED(_vibuffer->Render()))
		return E_FAIL;


	return S_OK;
}

HRESULT Sky::Ready_Components()
{
	if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), TEXT("Prototype_Component_Texture_Sky"),
		TEXT("Com_Texture"), reinterpret_cast<Component**>(&_texture))))
		return E_FAIL;

	if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), TEXT("Prototype_Component_Shader_VtxCube"),
		TEXT("Com_Shader"), reinterpret_cast<Component**>(&_shader))))
		return E_FAIL;

	if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), TEXT("Prototype_Component_VIBuffer_Cube"),
		TEXT("Com_VIBuffer"), reinterpret_cast<Component**>(&_vibuffer))))
		return E_FAIL;

	return S_OK;
}

HRESULT Sky::Bind_ShaderResources()
{
	if (FAILED(_transformCom->Bind_ShaderResource(_shader, "g_WorldMatrix")))
		return E_FAIL;

	if (FAILED(GAME->Bind_TransformMatrix(D3DTS::VIEW, _shader, "g_ViewMatrix")))
		return E_FAIL;

	if (FAILED(GAME->Bind_TransformMatrix(D3DTS::PROJ, _shader, "g_ProjMatrix")))
		return E_FAIL;

	if (FAILED(_texture->Bind_ShaderResourceView(_shader, "g_Texture", 2)))
		return E_FAIL;

	return S_OK;
}

Shared<Sky> Sky::Create(ComPtr<Device> device, ComPtr<Context> context)
{
	Shared<Sky> pInstance = MakeSharedFromRaw(new Sky(device, context));

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Sky");
		Safe_Release(pInstance);
	}
	return pInstance;
}


Shared<GameObject> Sky::Clone(void* pArg)
{
	Shared<Sky> pInstance = MakeSharedFromRaw(new Sky(*this));

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : Sky");
		Safe_Release(pInstance);
	}
	return pInstance;
}

void Sky::Free()
{
	__super::Free();

	
	Safe_Release(_shader);
	Safe_Release(_texture);
	Safe_Release(_vibuffer);
}


