#include "Sky.h"
#include "GameInstance.h"
#include "Camera.h"

IMPLEMENT_REFLECTION(Sky)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "Sky";
    info.category = "World";

    return true;
}

Sky::Sky(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: GameObject { device, context }
{
}

Sky::Sky(const Sky& prototype)
	: GameObject { prototype }

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

    _transformCom->Set_Scale(Vec3(100.f, 100.f, 100.f));

	return S_OK;
}

void Sky::Priority_Update(float fTimeDelta)
{
}

void Sky::Update(float fTimeDelta)
{
}

void Sky::Late_Update(float fTimeDelta)
{
	GAME->Add_RenderGroup(RenderGroup::Priority, GetSharedPtr<GameObject>());
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
	if (FAILED(__super::Add_Component(ETOI(LevelType::Static), TEXT("Texture_Sky"), _texture)))
		return E_FAIL;

	if (FAILED(__super::Add_Component(ETOI(LevelType::Static), TEXT("Shader_VtxCube"), _shader)))
		return E_FAIL;

	if (FAILED(__super::Add_Component(ETOI(LevelType::Static), TEXT("VIBuffer_Cube"), _vibuffer)))
		return E_FAIL;

	return S_OK;
}

HRESULT Sky::Bind_ShaderResources()
{
    Shared<Camera> activeCamera = GAME->Get_ActiveCamera();
    if (activeCamera != nullptr)
    {
        Shared<TransformCom> cameraTransform = activeCamera->Get_Transform();
        if (cameraTransform != nullptr)
        {
            _transformCom->Set_WorldPosition(cameraTransform->Get_WorldPosition());
        }
    }

	if (FAILED(_transformCom->Bind_ShaderResource(_shader.get(), "g_WorldMatrix")))
		return E_FAIL;

	if (FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix")))
		return E_FAIL;

	if (FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix")))
		return E_FAIL;

	if (FAILED(_texture->Bind_ShaderResourceView(_shader.get(), "g_Texture", 2)))
		return E_FAIL;

	return S_OK;
}

Shared<Sky> Sky::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
	auto pInstance = make_shared<Sky>(device, context);

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Sky");
		return nullptr;
	}
	return pInstance;
}


Shared<GameObject> Sky::Clone(void* pArg)
{
	auto pInstance = make_shared<Sky>(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : Sky");
		return nullptr;
	}
	return pInstance;
}

void Sky::Free()
{
	__super::Free();
}


