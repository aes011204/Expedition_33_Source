#include "pch.h"
#include "Hair_Player.h"
#include "GameInstance.h"
#include "Material.h"
#include "Material_Types.h"
#include "Player_WorldBase.h"
#include "ModelCom.h"

Hair_Player::Hair_Player(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : PlayableParts{ device, context } {}

Hair_Player::Hair_Player(const Hair_Player& prototype)
    : PlayableParts{ prototype } {}

HRESULT Hair_Player::Initialize_Prototype()
{
    return PlayableParts::Initialize_Prototype();
}

HRESULT Hair_Player::Initialize(void* arg)
{
    if (FAILED(PlayableParts::Initialize(arg)))
    {
        LOG_ERROR("[Hair_Player] : Failed to Initialize");
        return S_OK;
    }

    if (const HAIR_PLAYER_DESC* desc = static_cast<const HAIR_PLAYER_DESC*>(arg))
    {
        // PW_* 캐릭터에서 넘긴 hair physics 값을 실제 파츠에 저장
        _hairPhysicsDesc = desc->hairPhysicsDesc;
    }

    _offsetMatrix = Matrix::Identity;

    //Vec3 hairOffset = Vec3(17.61f, 0.0f, -2.765f);
    Vec3 hairOffset = Vec3(17.61f, -2.765f, 0.f);

    _transformCom->Set_Position(hairOffset);

    return S_OK;
}

void Hair_Player::BeginPlay()
{
    PlayableParts::BeginPlay();
    
}

void Hair_Player::Priority_Update(float timeDelta)
{
}

void Hair_Player::Update(float timeDelta)
{
    if (_modelCom == nullptr)
        return;

    Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix());

    if (!_hairPhysicsReady)
    {
        _hairPhysics.Initialize(_modelCom.get(), _hairPhysicsDesc);
        _hairPhysicsReady = _hairPhysics.Is_Built();
    }
         
    Vec3 ownerVelocity = Vec3::Zero;
    if (const Shared<GameObject> owner = Get_Owner())
    {
        if (const Player_WorldBase* player = dynamic_cast<const Player_WorldBase*>(owner.get()))
            ownerVelocity = player->Get_MovementVelocity();
    }

    if (_hairPhysicsReady)
        _hairPhysics.Simulate(_modelCom.get(), timeDelta, _combinedWorldMatrix, ownerVelocity);
}

void Hair_Player::Late_Update(float timeDelta)
{
    __super::Late_Update(timeDelta);
}

HRESULT Hair_Player::Render()
{
    return PlayableParts::Render();
}

HRESULT Hair_Player::Render_Shadow()
{
    return PlayableParts::Render_Shadow();
}

Shared<Hair_Player> Hair_Player::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Hair_Player>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("[Hair_Player] : Failed to Initialize Prototype");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> Hair_Player::Clone(void* arg)
{
    auto clone = make_shared<Hair_Player>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("[Hair_Player] : Failed to Initialize");
        return nullptr;
    }

    return clone;
}

void Hair_Player::Free()
{
    __super::Free();
}



