#include "Terrain.h"
#include "GameInstance.h"

IMPLEMENT_REFLECTION(Terrain)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "Terrain";
    info.category = "World";

    return true;
}

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

    if (FAILED(GAME->Bind_CameraFar(_shader, "g_fFar")))
        return E_FAIL;

	if (FAILED(_textures[TEXTURE::Diffuse]->Bind_ShaderResourceView(_shader.get(), "g_DiffuseTexture", 0)))
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
}


