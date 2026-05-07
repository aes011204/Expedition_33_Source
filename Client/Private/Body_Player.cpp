#include "Body_Player.h"
#include "GameInstance.h"
#include "Material.h"
#include "Material_Types.h"
#include "Player.h"
#include "ModelCom.h"

Body_Player::Body_Player(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: PlayableParts { device, context }
{
}

Body_Player::Body_Player(const Body_Player& prototype)
	: PlayableParts { prototype }
{
}

HRESULT Body_Player::Initialize_Prototype()
{
	return S_OK;
}

HRESULT Body_Player::Initialize(void* arg)
{
	if (FAILED(PlayableParts::Initialize(arg)))
	{
        MSG_BOX("[Body_Player] : Failed to Initialize");
        return E_FAIL;
	}
		

	_modelCom->Set_Animation(/*rand() % 20*/1, true);

	return S_OK;
}

void Body_Player::Priority_Update(float fTimeDelta)
{

}

void Body_Player::Update(float fTimeDelta)
{
    _modelCom->Play_Animation(fTimeDelta);

	Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix());
}

void Body_Player::Late_Update(float fTimeDelta)
{
	GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
	GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());

}

HRESULT Body_Player::Render()
{
	return PlayableParts::Render();
}

HRESULT Body_Player::Render_Shadow()
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
        _model->Bind_BoneMatrices(_shader.get(), "g_BoneMatrices", i);

        if (FAILED(_shader->Begin(1)))
            return E_FAIL;

        _model->Render(i);
    }

    return S_OK;
}

HRESULT Body_Player::Ready_Components()
{
	if (FAILED(Add_Component(ETOI(LevelType::GamePlay), TEXT("Shader_VtxAnimMesh"), _shader)))
		return E_FAIL;

	if (FAILED(Add_Component(ETOI(LevelType::GamePlay), TEXT("Model_LuneBody"), _model)))
		return E_FAIL;

	return S_OK;
}

HRESULT Body_Player::Bind_ShaderResources()
{
	/*if (FAILED(_transformCom->Bind_RT_ShaderResource(_states, "g_WorldMatrix")))
		return E_FAIL;*/
	if (FAILED(_shader->Bind_Matrix("g_WorldMatrix", &_combinedWorldMatrix)))
		return E_FAIL;

	if (FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix")))
		return E_FAIL;

	if (FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix")))
		return E_FAIL;

    if (FAILED(GAME->Bind_CameraFar(_shader, "g_fFar")))
        return E_FAIL;

    return PlayableParts::Render_Shadow();
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



