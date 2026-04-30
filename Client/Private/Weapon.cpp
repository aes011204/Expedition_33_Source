#include "Weapon.h"
#include "GameInstance.h"

#include "Player.h"

Weapon::Weapon(ComPtr<Device> device, ComPtr<Context> context)
	: PartObject { device, context }
{
}

Weapon::Weapon(const Weapon& Prototype)
	: PartObject{ Prototype }
{
}

HRESULT Weapon::Initialize_Prototype()
{
	return S_OK;
}

HRESULT Weapon::Initialize(void* pArg)
{
	auto	pDesc = static_cast<WEAPON_DESC*>(pArg);

	_parentState = pDesc->pParentState;
	_socketMatrix = pDesc->pSocketMatrix;


	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	if (FAILED(Ready_Components()))
		return E_FAIL;

	_transformCom->SetUp_Scale(0.1f, 0.1f, 0.1f);
	_transformCom->Rotation(XMVectorSet(0.f, 1.f, 0.f, 0.f), 90.f);
	_transformCom->Set_State(State::POSITION, XMVectorSet(0.8f, 0.f, 0.f, 1.f));

	/*_transformCom->Set_State(State::POSITION, XMVectorSet(
		GAME->Random(0.f, 10.f),
		2.f,
		GAME->Random(0.f, 10.f),
		1.f
	));*/


	return S_OK;
}

void Weapon::Priority_Update(float fTimeDelta)
{
}

void Weapon::Update(float fTimeDelta)
{	
	XMMATRIX		SocketMatrix = XMLoadFloat4x4(_socketMatrix);

	for (size_t i = 0; i < 3; i++)
		SocketMatrix.r[i] = XMVector3Normalize(SocketMatrix.r[i]);
	


	Update_CombinedWorldMatrix(XMLoadFloat4x4(_transformCom->Get_WorldMatrixPtr()) * SocketMatrix);
}

void Weapon::Late_Update(float fTimeDelta)
{
	GAME->Add_RenderGroup(RenderGroup::NONBLEND, this);
}

HRESULT Weapon::Render()
{
	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;



	size_t iNumMeshes = _model->Get_NumMeshes();

	for (size_t i = 0; i < iNumMeshes; i++)
	{
		_model->Bind_Material(_shader, "g_DiffuseTexture", i, aiTextureType_DIFFUSE, 0);		

		if (FAILED(_shader->Begin(0)))
			return E_FAIL;

		_model->Render(i);
	}


	return S_OK;
}

HRESULT Weapon::Ready_Components()
{

	if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), TEXT("Prototype_Component_Shader_VtxMesh"),
		TEXT("Com_Shader"), reinterpret_cast<Component**>(&_shader))))
		return E_FAIL;

	if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), TEXT("Prototype_Component_Model_ForkLift"),
		TEXT("Com_Model"), reinterpret_cast<Component**>(&_model))))
		return E_FAIL;

	return S_OK;
}

HRESULT Weapon::Bind_ShaderResources()
{
	/*if (FAILED(_transformCom->Bind_ShaderResource(_shader, "g_WorldMatrix")))
		return E_FAIL;*/
	if (FAILED(_shader->Bind_Matrix("g_WorldMatrix", &_combinedWorldMatrix)))
		return E_FAIL;

	if (FAILED(GAME->Bind_TransformMatrix(D3DTS::VIEW, _shader, "g_ViewMatrix")))
		return E_FAIL;

	if (FAILED(GAME->Bind_TransformMatrix(D3DTS::PROJ, _shader, "g_ProjMatrix")))
		return E_FAIL;



	if (FAILED(GAME->Bind_CamPosition(_shader, "g_vCamPosition")))
		return E_FAIL;

	const LIGHT_DESC* pLightDesc = GAME->Get_LightDesc(0);
	if (nullptr == pLightDesc)
		return E_FAIL;

	if (FAILED(_shader->Bind_RawValue("g_vLightDir", &pLightDesc->vDirection, sizeof(XMFLOAT4))))
		return E_FAIL;
	if (FAILED(_shader->Bind_RawValue("g_vLightDiffuse", &pLightDesc->vDiffuse, sizeof(XMFLOAT4))))
		return E_FAIL;
	if (FAILED(_shader->Bind_RawValue("g_vLightAmbient", &pLightDesc->vAmbient, sizeof(XMFLOAT4))))
		return E_FAIL;
	if (FAILED(_shader->Bind_RawValue("g_vLightSpecular", &pLightDesc->vSpecular, sizeof(XMFLOAT4))))
		return E_FAIL;

	return S_OK;
}

Shared<Weapon> Weapon::Create(ComPtr<Device> device, ComPtr<Context> context)
{
	Shared<Weapon> pInstance = MakeSharedFromRaw(new Weapon(device, context));

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Body_Player");
		Safe_Release(pInstance);
	}
	return pInstance;
}


Shared<GameObject> Weapon::Clone(void* pArg)
{
	Shared<Weapon> pInstance = MakeSharedFromRaw(new Weapon(*this));

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : Body_Player");
		Safe_Release(pInstance);
	}
	return pInstance;
}


void Weapon::Free()
{
	__super::Free();
	Safe_Release(_shader);
	Safe_Release(_model);

}


