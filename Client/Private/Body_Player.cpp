#include "Body_Player.h"
#include "GameInstance.h"

#include "Player.h"

Body_Player::Body_Player(ComPtr<Device> device, ComPtr<Context> context)
	: PartObject { device, context }
{
}

Body_Player::Body_Player(const Body_Player& Prototype)
	: PartObject{ Prototype }
{
}

const XMFLOAT4X4* Body_Player::Get_SocketBoneMatrixPtr(const char* pBoneName)
{
	return _model->Get_BoneMatrixPtr(pBoneName);
}

HRESULT Body_Player::Initialize_Prototype()
{
	return S_OK;
}

HRESULT Body_Player::Initialize(void* pArg)
{
	auto	pDesc = static_cast<BODY_PLAYER_DESC*>(pArg);

	_parentState = pDesc->pParentState;


	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;

	if (FAILED(Ready_Components()))
		return E_FAIL;

	/*_transformCom->Set_State(State::POSITION, XMVectorSet(
		GAME->Random(0.f, 10.f),
		2.f,
		GAME->Random(0.f, 10.f),
		1.f
	));*/

	_model->Set_Animation(/*rand() % 20*/0, true);

	return S_OK;
}

void Body_Player::Priority_Update(float fTimeDelta)
{
}

void Body_Player::Update(float fTimeDelta)
{

	if(*_parentState & Player::PLAYERSTATE::IDLE)
		_model->Set_Animation(3, true);

	if (*_parentState & Player::PLAYERSTATE::WALK)
		_model->Set_Animation(4, true);

	if (true == _model->Play_Animation(fTimeDelta))
		int a = 10;

	Update_CombinedWorldMatrix(XMLoadFloat4x4(_transformCom->Get_WorldMatrixPtr()));

}

void Body_Player::Late_Update(float fTimeDelta)
{
	GAME->Add_RenderGroup(RenderGroup::NONBLEND, this);
}

HRESULT Body_Player::Render()
{
	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;



	size_t iNumMeshes = _model->Get_NumMeshes();

	for (size_t i = 0; i < iNumMeshes; i++)
	{
		_model->Bind_Material(_shader, "g_DiffuseTexture", i, aiTextureType_DIFFUSE, 0);
		_model->Bind_BoneMatrices(_shader, "g_BoneMatrices", i);

		if (FAILED(_shader->Begin(0)))
			return E_FAIL;

		_model->Render(i);
	}


	return S_OK;
}

HRESULT Body_Player::Ready_Components()
{

	if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), TEXT("Prototype_Component_Shader_VtxAnimMesh"),
		TEXT("Com_Shader"), reinterpret_cast<Component**>(&_shader))))
		return E_FAIL;

	if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), TEXT("Prototype_Component_Model_Fiona"),
		TEXT("Com_Model"), reinterpret_cast<Component**>(&_model))))
		return E_FAIL;

	return S_OK;
}

HRESULT Body_Player::Bind_ShaderResources()
{
	/*if (FAILED(_transformCom->Bind_ShaderResource(_states, "g_WorldMatrix")))
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

Shared<Body_Player> Body_Player::Create(ComPtr<Device> device, ComPtr<Context> context)
{
	Shared<Body_Player> pInstance = MakeSharedFromRaw(new Body_Player(device, context));

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Body_Player");
		Safe_Release(pInstance);
	}
	return pInstance;
}


Shared<GameObject> Body_Player::Clone(void* pArg)
{
	Shared<Body_Player> pInstance = MakeSharedFromRaw(new Body_Player(*this));

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : Body_Player");
		Safe_Release(pInstance);
	}
	return pInstance;
}


void Body_Player::Free()
{
	__super::Free();
	Safe_Release(_shader);
	Safe_Release(_model);

}



