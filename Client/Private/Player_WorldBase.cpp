#include "Player_WorldBase.h"
#include "Weapon.h"
#include "GameInstance.h"
#include "ClientInstance.h"
#include "InputCom.h"
#include "MovementCom.h"
#include "PlayerControllerCom.h"
#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "PlayerStatCom.h"
#include "PlayableParts.h"
#include "PlayerStateMachineCom.h"
#include "Body_Player.h"
#include "EffectCom.h"
#include "FaceAccessory_Player.h"
#include "Hair_Player.h"
#include "Head_Player.h"
#include "Player_GameMenu.h"
#include "SkillTreeCom.h"

Player_WorldBase::Player_WorldBase(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Character_WorldBase{ device, context }
{
}

Player_WorldBase::Player_WorldBase(const Player_WorldBase& prototype)
    : Character_WorldBase{ prototype }
{
}

HRESULT Player_WorldBase::Initialize_Prototype()
{
    return S_OK;
}

HRESULT Player_WorldBase::Initialize(void* arg)
{
    Character_WorldBase::ContainerObjectDesc desc{};
    desc.speedPerSec = 20.f;
    desc.degreePerSec = 180.f;

    CHECK_FAILED(Character_WorldBase::Initialize(&desc), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    return S_OK;
}

void Player_WorldBase::BeginPlay()
{
    Character_WorldBase::BeginPlay();

    if (auto* body = dynamic_cast<Body_Player*>(Get_PartObject(L"Part_Body")))
    {
        if (Shared<ModelCom> model = body->Get_Model())
        {
            model->Refresh_AnimStateMappingFromOwnerContext();
        }
    }

    _playerStateMachineCom->Change_State(PlayerState::Idle);
}

void Player_WorldBase::Priority_Update(float timeDelta)
{
    if (CLIENT->Is_BattleActive())
        return;

    __super::Priority_Update(timeDelta);

    // follower도 StateMachine/Controller를 타야 Idle/Run/Sprint 전환과 PhysX 이동이 처리된다.
    _playerControllerCom->Update_InputState();
    _playerStateMachineCom->Update_CurrentState(timeDelta);
    _playerControllerCom->Update_Movement(timeDelta);
    _playerStateMachineCom->LateUpdate_CurrentState(timeDelta);
}

void Player_WorldBase::Update(float timeDelta)
{
    if (CLIENT->Is_BattleActive())
        return;

    __super::Update(timeDelta);

    if (_effectCom)
        _effectCom->Update(timeDelta);

}

void Player_WorldBase::Late_Update(float timeDelta)
{
    if (CLIENT->Is_BattleActive())
        return;

    __super::Late_Update(timeDelta);

    Update_Culling(Vec3(1.f, 2.f, 1.f));
}

HRESULT Player_WorldBase::Render()
{
    return S_OK;
}

void Player_WorldBase::Update_StatusPreview(float timeDelta)
{
    Try_BeginPlay();

    ContainerObject::Update(timeDelta);
    ContainerObject::Late_Update(timeDelta);
}

void Player_WorldBase::Enter_GameMenuAnimation(uint32 introAnim, uint32 idleAnim)
{
    if (nullptr == _playerStateMachineCom) return;

    _playerStateMachineCom->Enter_GameMenuAnimation(introAnim, idleAnim);
}

Vec3 Player_WorldBase::Get_MovementVelocity() const
{
    if (_movementCom == nullptr)
        return Vec3::Zero;

    return _movementCom->Get_Velocity();
}

void Player_WorldBase::Start_SwitchDissolve(float durationSec, const string& noiseName)
{
    if (_effectCom)
        _effectCom->Start_SwitchDissolve(durationSec, noiseName);


}

HRESULT Player_WorldBase::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Input"), _inputCom), E_FAIL);
       
    MovementCom::MoveDesc moveDesc{};
    moveDesc.walkSpeed = 3.f;
    moveDesc.runSpeed = 6.5f;
    moveDesc.sprintSpeed = 10.f;
    moveDesc.turnSpeedDegree = 720.f;
    moveDesc.turnResponse = 10.f;
    moveDesc.jumpVelocity = 20.f;
    moveDesc.gravity = -40.f;

    moveDesc.backend = MovementCom::MoveBackend::PhysXCCT; // 플레이어는 CCT로 테스트

    moveDesc.sweep.skinWidth = 0.03f;
    moveDesc.sweep.groundSnapUp = 0.35f;
    moveDesc.sweep.groundSnapDown = 0.65f;
    moveDesc.sweep.walkableNormalY = 0.65f;
    moveDesc.sweep.groundY = 0.f;

    moveDesc.cct.radius = 0.32f;
    moveDesc.cct.height = 1.50f;
    moveDesc.cct.stepOffset = 0.45f;
    moveDesc.cct.contactOffset = 0.05f;
    moveDesc.cct.slopeLimitDeg = 55.f;
    moveDesc.cct.groundLerpSpeed = 15.f;

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Movement"), _movementCom, &moveDesc), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_PlayerController"), _playerControllerCom), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_PlayerStateMachine"), _playerStateMachineCom), E_FAIL);
    
    Bounding_OBB::BoundingOBBDesc obbDesc{};
    obbDesc.center = Vec3(0.f, 0.75f, 0.f);
    obbDesc.extents = Vec3(0.35f, 0.75f, 0.35f);

    CHECK_FAILED(Ready_Collider(L"Com_Collider_OBB", &obbDesc, Collision_Preset::Player_WorldBody), E_FAIL);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_SkillTree"), _skillTreeCom), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_PlayerStat"), _combatStatCom), E_FAIL);

	CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Effect"), _effectCom), E_FAIL);

    return S_OK;
}

HRESULT Player_WorldBase::Ready_PartObjects()
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

    Head_Player* head = dynamic_cast<Head_Player*>(_partObjects[TEXT("Part_Head")].get());
    if (nullptr == head)
        return E_FAIL;

    // 머리붙이기
    head->Set_LeaderPoseComponent(mainBody);

    // 콧수염
    if (const wchar_t* beardModelTag = Get_BeardModelTag())
    {
        FaceAccessory_Player::FACE_ACCESSORY_DESC beardPartDesc{};
        beardPartDesc.parentMatrix = playableDesc.parentMatrix;
        beardPartDesc.modelType = ModelType::NonAnim;
        beardPartDesc.modelTag = beardModelTag;
        beardPartDesc.socketMatrixPointer = head->Get_SocketBoneMatrixPtr("head");

        if (FAILED(
            __super::Add_PartObject(
                ETOI(LevelType::Static),
                TEXT("GameObject_FaceAccessory_Player"),
                TEXT("Part_Beard"),
                &beardPartDesc)))
            return E_FAIL;
    }

    // 턱수염
    if (const wchar_t* mustacheModelTag = Get_MustacheModelTag())
    {
        FaceAccessory_Player::FACE_ACCESSORY_DESC mustachePartDesc{};
        mustachePartDesc.parentMatrix = playableDesc.parentMatrix;
        mustachePartDesc.modelType = ModelType::NonAnim;
        mustachePartDesc.modelTag = mustacheModelTag;
        mustachePartDesc.socketMatrixPointer = head->Get_SocketBoneMatrixPtr("head");

        if (FAILED(
            __super::Add_PartObject(
                ETOI(LevelType::Static),
                TEXT("GameObject_FaceAccessory_Player"),
                TEXT("Part_Mustache"),
                &mustachePartDesc)))
            return E_FAIL;
    }


    //>> Hair Part
    Hair_Player::HAIR_PLAYER_DESC hairPartDesc{};
    hairPartDesc.parentMatrix = playableDesc.parentMatrix;
    hairPartDesc.modelType = ModelType::Anim;
    hairPartDesc.modelTag = Get_HairModelTag();
    hairPartDesc.hairPhysicsDesc = Get_HairChainPhysicsDesc();
    hairPartDesc.socketMatrixPointer = head->Get_SocketBoneMatrixPtr("head");

    if (FAILED(
        __super::Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Hair_Player"),
            TEXT("Part_Hair"), &hairPartDesc)
    ))
        return E_FAIL;

    return S_OK;
}

Shared<Player_WorldBase> Player_WorldBase::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Player_WorldBase>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : Player_WorldBase");
        return nullptr;
    }
    return instance;
}

Shared<GameObject> Player_WorldBase::Clone(void* arg)
{
    auto instance = make_shared<Player_WorldBase>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : Player_WorldBase");
        return nullptr;
    }
    return instance;
}

void Player_WorldBase::Free()
{
    __super::Free();
}
