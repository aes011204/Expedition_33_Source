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

    //_offsetMatrix = Matrix::CreateRotationX(XMConvertToRadians(-90.f)) * Matrix::CreateTranslation(0.f, 0.f, -10.f);
    _offsetMatrix = Matrix::Identity;

	return S_OK;
}

void Head_Player::Priority_Update(float fTimeDelta)
{
}

void Head_Player::Update(float timeDelta)
{
    if (Has_LeaderPoseComponent())
    {
        Update_LeaderPoseMatrices();
    }
    else
    {
        _modelCom->Play_Animation(timeDelta);
    }

	Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix());
}

void Head_Player::Late_Update(float timeDelta)
{
    __super::Late_Update(timeDelta);
}

HRESULT Head_Player::Render()
{
    if (FAILED(Bind_ShaderResources()))
        return E_FAIL;

    const uint32 numMeshes = static_cast<uint32>(_modelCom->Get_NumMeshes());
    for (uint32 i = 0; i < numMeshes; ++i)
    {
        const Shared<Material> material = _modelCom->Get_Material(i);
        if (material == nullptr)
            continue;

        if (FAILED(_shaderCom->Bind_MaterialTextures(*material)))
            continue;

        if (FAILED(_shaderCom->Bind_CBufferData(material->Get_CB())))
            return E_FAIL;

        _modelCom->Bind_BoneMatrices(_shaderCom.get(), "g_BoneMatrices", i);

        const uint32 passIndex = material->Get_PassIndex();
        if (FAILED(_shaderCom->Begin(passIndex)))
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



