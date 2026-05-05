#include "Monster.h"
#include "GameInstance.h"
#include "Mon_AIController.h"
#include "ModelCom.h"
#include "ShaderCom.h"

Monster::Monster(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: Character { device, context }
{
}

Monster::Monster(const Monster& prototype)
	: Character { prototype }

{
}

HRESULT Monster::Initialize_Prototype()
{
	return S_OK;
}

HRESULT Monster::Initialize(void* arg)
{	
	if (FAILED(__super::Initialize(arg)))
		return E_FAIL;
	
	if (FAILED(Ready_Components()))
		return E_FAIL;

	//_transformCom->Set_Position(Vec3(
	//	GAME->Random(0.f, 10.f), 
	//	2.f, 
	//	GAME->Random(0.f, 10.f)		
	//));

    _transformCom->Set_Position(Vec3(
        10.f,2.f,10.f
    ));

	_model->Set_Animation(/*rand() % 20*/0, true);

	return S_OK;
}

void Monster::Priority_Update(float timeDelta)
{

}

void Monster::Update(float timeDelta)
{
	if (true == _model->Play_Animation(timeDelta))
		int a = 10;

    _aiCtrl->Update(timeDelta);
}

void Monster::Late_Update(float fTimeDelta)
{
 	GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<Character>());
	GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<Character>());
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

HRESULT Monster::Render_Shadow()
{
    if (FAILED(_transformCom->Bind_ShaderResource(_shader.get(), "g_WorldMatrix")))
        return E_FAIL;

    if (FAILED(GAME->Bind_ShadowMatrices(_shader, "g_ViewMatrix", "g_ProjMatrix")))
        return E_FAIL;

    if (FAILED(GAME->Bind_ShadowLightDesc(_shader, "g_fLightFar")))
        return E_FAIL;

    const uint32 iNumMeshes = static_cast<uint32>(_model->Get_NumMeshes());

    for (uint32 i = 0; i < iNumMeshes; i++)
    {
        _model->Bind_BoneMatrices(_shader.get(), "g_BoneMatrices", i);

        if (FAILED(_shader->Begin(1)))
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


    if (FAILED(__super::Add_Component(ETOI(LevelType::Static), TEXT("Com_MonAIController"), _aiCtrl)))
        return E_FAIL;

    _aiCtrl->Set_Owner(static_pointer_cast<Monster>(shared_from_this()));
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

    if (FAILED(GAME->Bind_CameraFar(_shader, "g_fFar")))
        return E_FAIL;

	return S_OK;
}

Shared<Monster> Monster::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Monster>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : Monster");
        return nullptr;
    }

    return instance;
}


Shared<GameObject> Monster::Clone(void* arg)
{
    auto clone = make_shared<Monster>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : Monster");
        return nullptr;
    }

    return clone;
}

void Monster::Free()
{
	__super::Free();

}


