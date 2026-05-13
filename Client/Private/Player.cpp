#include "Player.h"
#include "Weapon.h"
#include "GameInstance.h"
#include "InputCom.h"
#include "MovementCom.h"
#include "PlayerControllerCom.h"
#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "CombatStatCom.h"
#include "PlayableParts.h"
#include "PlayerStateMachineCom.h"
#include "Body_Player.h"
#include "Hair_Player.h"
#include "Head_Player.h"
#include "Player_Idle.h"
#include "Player_Jump.h"
#include "Player_Move.h"

Player::Player(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Character{ device, context }
{
}

Player::Player(const Player& prototype)
    : Character{ prototype }
{
}

HRESULT Player::Initialize_Prototype()
{
    return S_OK;
}

HRESULT Player::Initialize(void* arg)
{
    Character::ContainerObjectDesc desc{};
    desc.speedPerSec = 20.f;
    desc.degreePerSec = 180.f;

    if (FAILED(__super::Initialize(&desc)))
        return E_FAIL;

    if (FAILED(Ready_Components()))
        return E_FAIL;

    if (FAILED(Ready_PartObjects()))
        return E_FAIL;

    return S_OK;
}

void Player::BeginPlay()
{
    Character::BeginPlay();

    _PlayerStateMachineCom->Change_State(PlayerState::Idle);
}

void Player::Priority_Update(float timeDelta)
{
    __super::Priority_Update(timeDelta);
}

void Player::Update(float timeDelta)
{
    __super::Update(timeDelta);

    _playerControllerCom->Update_Controller(timeDelta);
    _PlayerStateMachineCom->Update_StateMachine(timeDelta);
    _currentState = _PlayerStateMachineCom->Get_CurrentPlayerState();
}

void Player::Late_Update(float timeDelta)
{
    __super::Late_Update(timeDelta);

    if (_colliderCom)
    {
        _colliderCom->Update_Collider(_transformCom->Get_WorldMatrix());
        GAME->Add_Collider(_colliderCom);
    }
}

HRESULT Player::Render()
{
    return S_OK;
}

void Player::Update_StatusPreview(float timeDelta)
{
    Try_BeginPlay();

    ContainerObject::Update(timeDelta);
    ContainerObject::Late_Update(timeDelta);
}

HRESULT Player::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Input"), _inputCom), E_FAIL);

    MovementCom::MoveDesc moveDesc{};
    moveDesc.walkSpeed = 9.f;
    moveDesc.runSpeed = 10.f;
    moveDesc.turnSpeedDegree = 360.f;
    moveDesc.jumpVelocity = 8.f;
    moveDesc.gravity = -20.f;
    moveDesc.groundY = 0.f;

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Movement"), _movementCom, &moveDesc), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_PlayerController"), _playerControllerCom), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_PlayerStateMachine"), _PlayerStateMachineCom), E_FAIL);
    if (const auto stateMachine = Get_Component<PlayerStateMachineCom>())
    {
      
    }
    CHECK_FAILED(Ready_Collider(), E_FAIL);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_CombatStat"), _combatStatCom), E_FAIL);

    return S_OK;
}

HRESULT Player::Ready_PartObjects()
{
    Body_Player::BODY_PLAYER_DESC playableDesc{};
    playableDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    playableDesc.modelType = ModelType::Anim;
    playableDesc.modelTag = Get_BodyModelTag();;

    //> MainBody Setting
    if (FAILED(
        __super::Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Body_Player"),
            TEXT("Part_Body"), &playableDesc)
    ))
        return E_FAIL;

    const Body_Player* mainBody = dynamic_cast<Body_Player*>(_partObjects[TEXT("Part_Body")].get());
    if (nullptr == mainBody)
        return E_FAIL;

    //> MainBody에 붙을 파츠 세팅
    //>> Head Part
    Head_Player::HEAD_PLAYER_DESC headPartDesc{};
    headPartDesc.parentMatrix = playableDesc.parentMatrix;
    headPartDesc.modelType = ModelType::Anim;
    headPartDesc.modelTag = Get_HeadModelTag();
    //headPartDesc.socketMatrixPointer = mainBody->Get_SocketBoneMatrixPtr("Metahuman_Head");
    headPartDesc.socketMatrixPointer = nullptr;

    if (FAILED(
        __super::Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Head_Player"),
            TEXT("Part_Head"), &headPartDesc)
    ))
        return E_FAIL;

    const Head_Player* head = dynamic_cast<Head_Player*>(_partObjects[TEXT("Part_Head")].get());
    if (nullptr == head)
        return E_FAIL;

    // 머리붙이기
    head->Set_LeaderPoseComponent(mainBody);

    //>> Hair Part
    Hair_Player::HAIR_PLAYER_DESC hairPartDesc{};
    hairPartDesc.parentMatrix = playableDesc.parentMatrix;
    hairPartDesc.modelType = ModelType::Anim;
    hairPartDesc.modelTag = Get_HairModelTag();
    hairPartDesc.socketMatrixPointer = head->Get_SocketBoneMatrixPtr("head");

    if (FAILED(
        __super::Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Hair_Player"),
            TEXT("Part_Hair"), &hairPartDesc)
    ))
        return E_FAIL;

    return S_OK;
}

HRESULT Player::Ready_Collider()
{
    Bounding_OBB::BoundingOBBDesc obbDesc{};
    obbDesc.center = Vec3(0.f, 0.5f, 0.f);
    obbDesc.extents = Vec3(0.5f, 0.6f, 0.5f);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), L"Com_Collider_OBB", _colliderCom, &obbDesc), E_FAIL);
    _colliderCom->Set_CollisionPreset(Collision_Preset::Player_Body);
    _colliderCom->Set_IsActive(true);

    return S_OK;
}

Shared<Player> Player::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Player>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : Player");
        return nullptr;
    }
    return instance;
}

Shared<GameObject> Player::Clone(void* arg)
{
    auto instance = make_shared<Player>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : Player");
        return nullptr;
    }
    return instance;
}

void Player::Free()
{
    __super::Free();
}
