#include "Head_Player.h"
#include "GameInstance.h"
#include "Material.h"
#include "Material_Types.h"
#include "Player.h"
#include "ModelCom.h"

Head_Player::Head_Player(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: PlayableParts{ device, context }
{
}

Head_Player::Head_Player(const Head_Player& prototype)
	: PlayableParts{ prototype }
{
}

HRESULT Head_Player::Initialize_Prototype()
{
	return PlayableParts::Initialize_Prototype();
}

HRESULT Head_Player::Initialize(void* arg)
{
	if (FAILED(PlayableParts::Initialize(arg)))
	{
        LOG_ERROR("[Head_Player] : Failed to Initialize");
        return S_OK;
	}

    

	return S_OK;
}

void Head_Player::Priority_Update(float fTimeDelta)
{
}

void Head_Player::Update(float timeDelta)
{
    _modelCom->Play_Animation(timeDelta);
	Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix());
}

void Head_Player::Late_Update(float timeDelta)
{
	GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
	GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());
}

HRESULT Head_Player::Render()
{
    //return PlayableParts::Render();

    if (FAILED(Bind_ShaderResources()))
        return E_FAIL;

    const uint32 numMeshes = static_cast<uint32>(_modelCom->Get_NumMeshes());
    for (uint32 i = 0; i < numMeshes; i++)
    {
        _modelCom->Bind_Material(_shaderCom.get(), "g_DiffuseTexture", i, MaterialTextureSlot::BaseColor);

        const Material::TextureSlot* baseColorSlot = _modelCom->Find_MaterialSlot(i, { MaterialTextureSlot::BaseColor });
        const Vec2 uvScale = Vec2(1.f, 1.f); //baseColorSlot ? baseColorSlot->uvScale : Vec2(1.f, 1.f);
        const Vec2 uvOffset = Vec2(0.f, 0.f); //baseColorSlot ? baseColorSlot->uvOffset : Vec2(0.f, 0.f);
        const float uvRotation = 0.f; // baseColorSlot ? baseColorSlot->uvRotation : 0.f;
        const int32 useOpacityMask = 0;

        _shaderCom->Bind_RawValue("g_UVScale", &uvScale, sizeof(Vec2));
        _shaderCom->Bind_RawValue("g_UVOffset", &uvOffset, sizeof(Vec2));
        _shaderCom->Bind_RawValue("g_UVRotation", &uvRotation, sizeof(float));
        _shaderCom->Bind_RawValue("g_UseOpacityMask", &useOpacityMask, sizeof(int32));

        _modelCom->Bind_BoneMatrices(_shaderCom.get(), "g_BoneMatrices", i);

        if (FAILED(_shaderCom->Begin(0)))
            return E_FAIL;

        _modelCom->Render(i);
    }

    return S_OK;
}

HRESULT Head_Player::Render_Shadow()
{
    return PlayableParts::Render_Shadow();
}

Shared<Head_Player> Head_Player::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Head_Player>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : Head_Player");
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



