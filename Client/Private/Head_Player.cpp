#include "Head_Player.h"
#include "GameInstance.h"
#include "Material.h"
#include "Material_Types.h"
#include "Player.h"
#include "ModelCom.h"

Head_Player::Head_Player(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: PartObject { device, context }
{
}

Head_Player::Head_Player(const Head_Player& prototype)
	: PartObject{ prototype }
{
}

const XMFLOAT4X4* Head_Player::Get_SocketBoneMatrixPtr(const char* pBoneName)
{
	return _model->Get_BoneMatrixPtr(pBoneName);
}

HRESULT Head_Player::Initialize_Prototype()
{
	return S_OK;
}

HRESULT Head_Player::Initialize(void* pArg)
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

void Head_Player::Priority_Update(float fTimeDelta)
{
}

void Head_Player::Update(float fTimeDelta)
{
	if (true == _model->Play_Animation(fTimeDelta))
		int a = 10;

	Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix());

}

void Head_Player::Late_Update(float fTimeDelta)
{
	GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
	GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());

}

HRESULT Head_Player::Render()
{
	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;

	const uint32 iNumMeshes = static_cast<uint32>(_model->Get_NumMeshes());

	for (uint32 i = 0; i < iNumMeshes; i++)
    {
        if (FAILED(_model->Bind_Material(_shader.get(), "g_DiffuseTexture", i, MaterialTextureSlot::BaseColor)))
            continue;

        _model->Bind_Material(_shader.get(), "g_NormalTexture", i, MaterialTextureSlot::Normal);
        _model->Bind_Material(_shader.get(), "g_EmissiveTexture", i, MaterialTextureSlot::Emissive);

        const Material::TextureSlot* baseColorSlot =
            _model->Find_MaterialSlot(i, { MaterialTextureSlot::BaseColor });
        const Vec2 uvScale = baseColorSlot ? baseColorSlot->uvScale : Vec2(1.f, 1.f);
        const Vec2 uvOffset = baseColorSlot ? baseColorSlot->uvOffset : Vec2(0.f, 0.f);
        const float uvRotation = baseColorSlot ? baseColorSlot->uvRotation : 0.f;
        const int useOpacityMask = 0;

        _shader->Bind_RawValue("g_UVScale", &uvScale, sizeof(Vec2));
        _shader->Bind_RawValue("g_UVOffset", &uvOffset, sizeof(Vec2));
        _shader->Bind_RawValue("g_UVRotation", &uvRotation, sizeof(float));
        _shader->Bind_RawValue("g_UseOpacityMask", &useOpacityMask, sizeof(int));

        _model->Bind_BoneMatrices(_shader.get(), "g_BoneMatrices", i);

        if (FAILED(_shader->Begin(0)))
            return E_FAIL;

        _model->Render(i);
    }


	return S_OK;
}

HRESULT Head_Player::Render_Shadow()
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

HRESULT Head_Player::Ready_Components()
{
	if (FAILED(Add_Component(ETOI(LevelType::GamePlay), TEXT("Shader_VtxAnimMesh"), _shader)))
		return E_FAIL;

	if (FAILED(Add_Component(ETOI(LevelType::GamePlay), TEXT("Model_LuneTemp"), _model)))
		return E_FAIL;

	return S_OK;
}

HRESULT Head_Player::Bind_ShaderResources()
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

	return S_OK;
}

Shared<Head_Player> Head_Player::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Head_Player>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : Head_Player");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> Head_Player::Clone(void* arg)
{
    auto clone = make_shared<Head_Player>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : Head_Player");
        return nullptr;
    }

    return clone;
}

void Head_Player::Free()
{
	__super::Free();


}



