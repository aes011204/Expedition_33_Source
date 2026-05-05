#include "Weapon.h"
#include "GameInstance.h"

#include "Player.h"

Weapon::Weapon(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: PartObject { device, context }
{
}

Weapon::Weapon(const Weapon& prototype)
	: PartObject{ prototype }
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

	_transformCom->Set_Scale(Vec3(0.001f, 0.001f, 0.001f));
	_transformCom->Rotate(Vec3::Up, 90.f);
	_transformCom->Set_Position(Vec3(0.8f, 0.f, 0.f));

	return S_OK;
}

void Weapon::Priority_Update(float fTimeDelta)
{
}

void Weapon::Update(float fTimeDelta)
{	
    auto identityMat = Matrix::Identity;
    _socketMatrix = &identityMat;

	XMMATRIX		SocketMatrix = XMLoadFloat4x4(_socketMatrix);

	for (size_t i = 0; i < 3; i++)
		SocketMatrix.r[i] = XMVector3Normalize(SocketMatrix.r[i]);
	


	Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix() * SocketMatrix);
}

void Weapon::Late_Update(float fTimeDelta)
{
	GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
	GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());
}

HRESULT Weapon::Render()
{
	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;

	const uint32 iNumMeshes = static_cast<uint32>(_model->Get_NumMeshes());

	for (uint32 i = 0; i < iNumMeshes; i++)
	{
		_model->Bind_Material(_shader.get(), "g_DiffuseTexture", i, 1, 0);

		if (FAILED(_shader->Begin(0)))
			return E_FAIL;

		_model->Render(i);
	}


	return S_OK;
}

HRESULT Weapon::Render_Shadow()
{
    if (FAILED(_shader->Bind_Matrix("g_WorldMatrix", &_combinedWorldMatrix)))
        return E_FAIL;

    if (FAILED(GAME->Bind_ShadowMatrices(_shader, "g_ViewMatrix", "g_ProjMatrix")))
        return E_FAIL;

    if (FAILED(GAME->Bind_ShadowLightDesc(_shader, "g_fLightFar")))
        return E_FAIL;

    const uint32 iNumMeshes = static_cast<uint32>(_model->Get_NumMeshes());

    for (uint32 i = 0; i < iNumMeshes; i++)
    {
        _model->Bind_Material(_shader.get(), "g_DiffuseTexture", i, 1, 0);

        if (FAILED(_shader->Begin(1)))
            return E_FAIL;

        _model->Render(i);
    }
    return S_OK;
}

HRESULT Weapon::Ready_Components()
{

	if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), TEXT("Shader_VtxMesh"), _shader)))
		return E_FAIL;

	if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), TEXT("Model_ForkLift"), _model)))
		return E_FAIL;

	return S_OK;
}

HRESULT Weapon::Bind_ShaderResources()
{
	/*if (FAILED(_transformCom->Bind_ShaderResource(_shader, "g_WorldMatrix")))
		return E_FAIL;*/
	if (FAILED(_shader->Bind_Matrix("g_WorldMatrix", &_combinedWorldMatrix)))
		return E_FAIL;

	if (FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix")))
		return E_FAIL;

	if (FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix")))
		return E_FAIL;

    if (FAILED(GAME->Bind_CameraFar(_shader, "g_fFar")))
        return E_FAIL;


	return S_OK;
}

Shared<Weapon> Weapon::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
	auto pInstance = make_shared<Weapon>(device, context);

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Body_Player");
		return nullptr;
	}
	return pInstance;
}


Shared<GameObject> Weapon::Clone(void* pArg)
{
	auto pInstance = make_shared<Weapon>(*this);

	if (FAILED(pInstance->Initialize(pArg)))
	{
		MSG_BOX("Failed to Cloned : Body_Player");
		return nullptr;
	}
	return pInstance;
}


void Weapon::Free()
{
	__super::Free();

}


