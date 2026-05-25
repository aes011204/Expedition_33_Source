#include "Head_Player.h"
#include "GameInstance.h"
#include "Material.h"
#include "Material_Types.h"
#include "Player_WorldBase.h"
#include "ModelCom.h"
#include "Debug_Profile.h"
#include "AnimComputeRuntime.h"

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

    _offsetMatrix = Matrix::Identity;

	return S_OK;
}

void Head_Player::Priority_Update(float fTimeDelta)
{
}

void Head_Player::Update(float timeDelta)
{
    PlayableParts::Update(timeDelta);

    Debug_Profile::Scope updateScope("Player::Head::Update");

    if (Has_LeaderPoseComponent())
    {
        Debug_Profile::Scope profileScope("Player::Head::LeaderPose");
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
    Debug_Profile::Scope profileScope("Player::Head::Render");

    return PlayableParts::Render();
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



