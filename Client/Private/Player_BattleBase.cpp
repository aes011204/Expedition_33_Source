#include "Player_BattleBase.h"
#include "Body_Player.h"
#include "Hair_Player.h"
#include "Head_Player.h"
#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "GameInstance.h"
#include "PlayerStatCom.h"

Player_BattleBase::Player_BattleBase(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Character(device, context)
{
}

Player_BattleBase::Player_BattleBase(const Player_BattleBase& prototype)
    : Character(prototype)
{
}

HRESULT Player_BattleBase::Initialize_Prototype()
{
    return Character::Initialize_Prototype();
}

HRESULT Player_BattleBase::Initialize(void* arg)
{
    Character::ContainerObjectDesc desc{};
    desc.speedPerSec = 0.f;
    desc.degreePerSec = 0.f;

    CHECK_FAILED(Character::Initialize(&desc), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    return S_OK;
}

void Player_BattleBase::BeginPlay()
{
    Character::BeginPlay();
}

void Player_BattleBase::Update(float timeDelta)
{
    Character::Update(timeDelta);
}

void Player_BattleBase::Late_Update(float timeDelta)
{
    Character::Late_Update(timeDelta);

    if (_colliderCom)
    {
        _colliderCom->Update_Collider(_transformCom->Get_WorldMatrix());
        GAME->Add_Collider(_colliderCom);
    }
}

HRESULT Player_BattleBase::Ready_PartObjects()
{
    Body_Player::BODY_PLAYER_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = Get_BodyModelTag();

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Body_Player"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    const Body_Player* body = dynamic_cast<Body_Player*>(_partObjects[TEXT("Part_Body")].get());
    CHECK_NULL(body, E_FAIL);

    Head_Player::HEAD_PLAYER_DESC headDesc{};
    headDesc.parentMatrix = bodyDesc.parentMatrix;
    headDesc.modelType = ModelType::Anim;
    headDesc.modelTag = Get_HeadModelTag();
    headDesc.socketMatrixPointer = nullptr;

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Head_Player"), TEXT("Part_Head"), &headDesc), E_FAIL);

    const Head_Player* head = dynamic_cast<Head_Player*>(_partObjects[TEXT("Part_Head")].get());
    CHECK_NULL(head, E_FAIL);
    head->Set_LeaderPoseComponent(body);

    Hair_Player::HAIR_PLAYER_DESC hairDesc{};
    hairDesc.parentMatrix = bodyDesc.parentMatrix;
    hairDesc.modelType = ModelType::Anim;
    hairDesc.modelTag = Get_HairModelTag();
    hairDesc.hairPhysicsDesc = Get_HairChainPhysicsDesc();
    hairDesc.socketMatrixPointer = head->Get_SocketBoneMatrixPtr("head");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Hair_Player"), TEXT("Part_Hair"), &hairDesc), E_FAIL);

    return S_OK;
}

HRESULT Player_BattleBase::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_PlayerStat"), _combatStatCom), E_FAIL);
    CHECK_FAILED(Ready_Collider(), E_FAIL);
    return S_OK;
}

HRESULT Player_BattleBase::Ready_Collider()
{
    Bounding_OBB::BoundingOBBDesc obbDesc{};
    obbDesc.center = Vec3(0.f, 0.75f, 0.f);
    obbDesc.extents = Vec3(0.35f, 0.75f, 0.35f);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), L"Com_Collider_OBB", _colliderCom, &obbDesc), E_FAIL);
    _colliderCom->Set_CollisionPreset(Collision_Preset::Player_Body);
    _colliderCom->Set_IsActive(true);
    return S_OK;
}
