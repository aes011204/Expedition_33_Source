#include "Monster.h"
#include "GameInstance.h"

Monster::Monster(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: GameObject { device, context }
{
}

Monster::Monster(const Monster& prototype)
	: GameObject { prototype }

{
}

HRESULT Monster::Initialize_Prototype()
{
	return S_OK;
}

HRESULT Monster::Initialize(void* pArg)
{	
	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;
	
	if (FAILED(Ready_Components()))
		return E_FAIL;

	_transformCom->Set_Position(Vec3(
		GAME->Random(0.f, 10.f), 
		2.f, 
		GAME->Random(0.f, 10.f)		
	));

	_model->Set_Animation(/*rand() % 20*/0, true);

	return S_OK;
}

void Monster::Priority_Update(float fTimeDelta)
{

}

void Monster::Update(float fTimeDelta)
{
	if (true == _model->Play_Animation(fTimeDelta))
		int a = 10;
}

void Monster::Late_Update(float fTimeDelta)
{
 	GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
}

HRESULT Monster::Render()
{
	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;



	const uint32 iNumMeshes = static_cast<uint32>(_model->Get_NumMeshes());

	for (uint32 i = 0; i < iNumMeshes; i++)
	{
		_model->Bind_Material(_shader.get(), "g_DiffuseTexture", i, aiTextureType_DIFFUSE, 0);
		_model->Bind_BoneMatrices(_shader.get(), "g_BoneMatrices", i);
		
		if (FAILED(_shader->Begin(0)))
			return E_FAIL;

		_model->Render(i);
	}


	


	return S_OK;
}

HRESULT Monster::Ready_Components()
{


	if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), TEXT("Shader_VtxAnimMesh"), _shader)))
		return E_FAIL;

	if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), TEXT("Model_Fiona"), _model)))
		return E_FAIL;

	return S_OK;
}

HRESULT Monster::Bind_ShaderResources()
{
	if (FAILED(_transformCom->Bind_ShaderResource(_shader.get(), "g_WorldMatrix")))
		return E_FAIL;

	if (FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix")))
		return E_FAIL;

	if (FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix")))
		return E_FAIL;



	if (FAILED(GAME->Bind_CamPosition(_shader.get(), "g_vCamPosition")))
		return E_FAIL;

	const LIGHT_DESC*		pLightDesc = GAME->Get_LightDesc(0);
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

Shared<Monster> Monster::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
	auto pInstance = make_shared<Monster>(device, context);

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Monster");
		return nullptr;
	}
	return pInstance;
}


Shared<GameObject> Monster::Clone(void* pArg)
{
	auto pInstance = make_shared<Monster>(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : Monster");
		return nullptr;
	}
	return pInstance;
}

void Monster::Free()
{
	__super::Free();

	
	Safe_Release(_shader);	
	Safe_Release(_model);
}


