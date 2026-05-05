#include "Body_Player.h"
#include "GameInstance.h"

#include "Player.h"

Body_Player::Body_Player(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: PartObject { device, context }
{
}

Body_Player::Body_Player(const Body_Player& prototype)
	: PartObject{ prototype }
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

	_model->Set_Animation(/*rand() % 20*/1, true);

	return S_OK;
}

void Body_Player::Priority_Update(float fTimeDelta)
{
}

void Body_Player::Update(float fTimeDelta)
{
	if (true == _model->Play_Animation(fTimeDelta))
		int a = 10;

	Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix());

}

void Body_Player::Late_Update(float fTimeDelta)
{
	GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
}

HRESULT Body_Player::Render()
{
	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;

	const uint32 iNumMeshes = static_cast<uint32>(_model->Get_NumMeshes());

	for (uint32 i = 0; i < iNumMeshes; i++)
	{
		_model->Bind_Material(_shader.get(), "g_DiffuseTexture", i, 1, 0);
		_model->Bind_BoneMatrices(_shader.get(), "g_BoneMatrices", i);

		if (FAILED(_shader->Begin(0)))
			return E_FAIL;

		_model->Render(i);
	}

	return S_OK;
}

HRESULT Body_Player::Ready_Components()
{
	if (FAILED(Add_Component(ETOI(LevelType::GamePlay), TEXT("Shader_VtxAnimMesh"), _shader)))
		return E_FAIL;

	if (FAILED(Add_Component(ETOI(LevelType::GamePlay), TEXT("Model_LuneTemp"), _model)))
		return E_FAIL;

	return S_OK;
}

HRESULT Body_Player::Bind_ShaderResources()
{
	/*if (FAILED(_transformCom->Bind_ShaderResource(_states, "g_WorldMatrix")))
		return E_FAIL;*/
	if (FAILED(_shader->Bind_Matrix("g_WorldMatrix", &_combinedWorldMatrix)))
		return E_FAIL;

	if (FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix")))
		return E_FAIL;

	if (FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix")))
		return E_FAIL;



	if (FAILED(GAME->Bind_CamPosition(_shader.get(), "g_vCamPosition")))
		return E_FAIL;

	const LIGHT_DESC* pLightDesc = GAME->Get_LightDesc(0);
	if (nullptr == pLightDesc)
		return E_FAIL;

	if (FAILED(_shader->Bind_RawValue("g_vLightDir", &pLightDesc->direction, sizeof(XMFLOAT4))))
		return E_FAIL;
	if (FAILED(_shader->Bind_RawValue("g_vLightDiffuse", &pLightDesc->diffuse, sizeof(XMFLOAT4))))
		return E_FAIL;
	if (FAILED(_shader->Bind_RawValue("g_vLightAmbient", &pLightDesc->ambient, sizeof(XMFLOAT4))))
		return E_FAIL;
	if (FAILED(_shader->Bind_RawValue("g_vLightSpecular", &pLightDesc->specular, sizeof(XMFLOAT4))))
		return E_FAIL;

	return S_OK;
}

Shared<Body_Player> Body_Player::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Body_Player>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : Body_Player");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> Body_Player::Clone(void* arg)
{
    auto clone = make_shared<Body_Player>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : Body_Player");
        return nullptr;
    }

    return clone;
}

void Body_Player::Free()
{
	__super::Free();


}



