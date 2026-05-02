#include "Terrain.h"
#include "GameInstance.h"

Terrain::Terrain(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: GameObject { device, context }
{
}

Terrain::Terrain(const Terrain& prototype)
	: GameObject { prototype }

{
}

HRESULT Terrain::Initialize_Prototype()
{
	return S_OK;
}

HRESULT Terrain::Initialize(void* pArg)
{	
	if (FAILED(__super::Initialize(pArg)))
		return E_FAIL;
	
	if (FAILED(Ready_Components()))
		return E_FAIL;

	return S_OK;
}

void Terrain::Priority_Update(float fTimeDelta)
{
	int a = 10;
}

void Terrain::Update(float fTimeDelta)
{

}

void Terrain::Late_Update(float fTimeDelta)
{
 	GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
}

HRESULT Terrain::Render()
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

HRESULT Terrain::Ready_Components()
{
	if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), TEXT("Texture_Terrain"), _textures[TEXTURE::Diffuse])))
		return E_FAIL;
	if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), TEXT("Texture_Terrain_Mask"), _textures[TEXTURE::Mask])))
		return E_FAIL;
	if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), TEXT("Texture_Terrain_Brush"), _textures[TEXTURE::Brush])))
		return E_FAIL;

	if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), TEXT("Shader_VtxNorTex"), _shader)))
		return E_FAIL;

	if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), TEXT("VIBuffer_Terrain"), _vibuffer)))
		return E_FAIL;

	return S_OK;
}

HRESULT Terrain::Bind_ShaderResources()
{
	if (FAILED(_transformCom->Bind_ShaderResource(_shader.get(), "g_WorldMatrix")))
		return E_FAIL;

	if (FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix")))
		return E_FAIL;

	if (FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix")))
		return E_FAIL;

	if (FAILED(_textures[TEXTURE::Diffuse]->Bind_ShaderResourceViews(_shader.get(), "g_DiffuseTexture")))
		return E_FAIL;
	if (FAILED(_textures[TEXTURE::Mask]->Bind_ShaderResourceView(_shader.get(), "g_MaskTexture", 0)))
		return E_FAIL;
	if (FAILED(_textures[TEXTURE::Brush]->Bind_ShaderResourceView(_shader.get(), "g_BrushTexture", 0)))
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

Shared<Terrain> Terrain::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
	auto pInstance = make_shared<Terrain>(device, context);

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Terrain");
		return nullptr;
	}
	return pInstance;
}


Shared<GameObject> Terrain::Clone(void* pArg)
{
	auto pInstance = make_shared<Terrain>(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : Terrain");
		return nullptr;
	}
	return pInstance;
}

void Terrain::Free()
{
	__super::Free();

	
	Safe_Release(_shader);

	for (auto& pTextureCom : _textures)
		Safe_Release(pTextureCom);
	
	Safe_Release(_vibuffer);
}


