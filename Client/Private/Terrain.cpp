#include "Terrain.h"
#include "GameInstance.h"

Terrain::Terrain(ComPtr<Device> device, ComPtr<Context> context)
	: GameObject { device, context }
{
}

Terrain::Terrain(const Terrain& Prototype)
	: GameObject { Prototype }

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
 	GAME->Add_RenderGroup(RenderGroup::NONBLEND, this);
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
	if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), TEXT("Prototype_Component_Texture_Terrain"),
		TEXT("Com_Texture_Diffuse"), reinterpret_cast<Component**>(&_textures[TEXTURE::DIFFUSE]))))
		return E_FAIL;
	if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), TEXT("Prototype_Component_Texture_Terrain_Mask"),
		TEXT("Com_Texture_Mask"), reinterpret_cast<Component**>(&_textures[TEXTURE::MASK]))))
		return E_FAIL;
	if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), TEXT("Prototype_Component_Texture_Terrain_Brush"),
		TEXT("Com_Texture_Brush"), reinterpret_cast<Component**>(&_textures[TEXTURE::BRUSH]))))
		return E_FAIL;

	if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), TEXT("Prototype_Component_Shader_VtxNorTex"),
		TEXT("Com_Shader"), reinterpret_cast<Component**>(&_shader))))
		return E_FAIL;

	if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), TEXT("Prototype_Component_VIBuffer_Terrain"),
		TEXT("Com_VIBuffer"), reinterpret_cast<Component**>(&_vibuffer))))
		return E_FAIL;

	return S_OK;
}

HRESULT Terrain::Bind_ShaderResources()
{
	if (FAILED(_transformCom->Bind_ShaderResource(_shader, "g_WorldMatrix")))
		return E_FAIL;

	if (FAILED(GAME->Bind_TransformMatrix(D3DTS::VIEW, _shader, "g_ViewMatrix")))
		return E_FAIL;

	if (FAILED(GAME->Bind_TransformMatrix(D3DTS::PROJ, _shader, "g_ProjMatrix")))
		return E_FAIL;

	if (FAILED(_textures[TEXTURE::DIFFUSE]->Bind_ShaderResourceViews(_shader, "g_DiffuseTexture")))
		return E_FAIL;
	if (FAILED(_textures[TEXTURE::MASK]->Bind_ShaderResourceView(_shader, "g_MaskTexture", 0)))
		return E_FAIL;
	if (FAILED(_textures[TEXTURE::BRUSH]->Bind_ShaderResourceView(_shader, "g_BrushTexture", 0)))
		return E_FAIL;

	if (FAILED(GAME->Bind_CamPosition(_shader, "g_vCamPosition")))
		return E_FAIL;

	const LIGHT_DESC*		pLightDesc = GAME->Get_LightDesc(0);
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

Shared<Terrain> Terrain::Create(ComPtr<Device> device, ComPtr<Context> context)
{
	Shared<Terrain> pInstance = MakeSharedFromRaw(new Terrain(device, context));

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Terrain");
		Safe_Release(pInstance);
	}
	return pInstance;
}


Shared<GameObject> Terrain::Clone(void* pArg)
{
	Shared<Terrain> pInstance = MakeSharedFromRaw(new Terrain(*this));

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : Terrain");
		Safe_Release(pInstance);
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


