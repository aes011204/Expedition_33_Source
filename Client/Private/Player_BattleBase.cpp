#include "Player_BattleBase.h"
#include "Body_Player.h"
#include "Hair_Player.h"
#include "Head_Player.h"
#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "GameInstance.h"
#include "PlayerStatCom.h"
#include "Weapon.h"


Player_BattleBase::Player_BattleBase(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Character_BattleBase(device, context)
{
}

Player_BattleBase::Player_BattleBase(const Player_BattleBase& prototype)
    : Character_BattleBase(prototype)
{
}

HRESULT Player_BattleBase::Initialize_Prototype()
{
    return Character_BattleBase::Initialize_Prototype();
}

HRESULT Player_BattleBase::Initialize(void* arg)
{
    Character_BattleBase::ContainerObjectDesc desc{};
    desc.speedPerSec = 0.f;
    desc.degreePerSec = 0.f;

    CHECK_FAILED(Character_BattleBase::Initialize(&desc), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    return S_OK;
}

void Player_BattleBase::BeginPlay()
{
    Character_BattleBase::BeginPlay();

    if (auto* body = dynamic_cast<Body_Player*>(Get_PartObject(L"Part_Body")))
    {
        if (Shared<ModelCom> model = body->Get_Model())
        {
            model->Refresh_AnimStateMappingFromOwnerContext();
        }
    }

}

void Player_BattleBase::Update(float timeDelta)
{
    Character_BattleBase::Update(timeDelta);
}

void Player_BattleBase::Late_Update(float timeDelta)
{
    Character_BattleBase::Late_Update(timeDelta);
    
}

HRESULT Player_BattleBase::Ready_PartObjects()
{
    Body_Player::BODY_PLAYER_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = Get_BodyModelTag();

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Body_Player"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    Body_Player* body = dynamic_cast<Body_Player*>(_partObjects[TEXT("Part_Body")].get());
    CHECK_NULL(body, E_FAIL);

    Head_Player::HEAD_PLAYER_DESC headDesc{};
    headDesc.parentMatrix = bodyDesc.parentMatrix;
    headDesc.modelType = ModelType::Anim;
    headDesc.modelTag = Get_HeadModelTag();
    headDesc.socketMatrixPointer = nullptr; // 처음 1회는 LeaderPose 정렬값을 캡처하므로 아직 socket을 바로 쓰지 않는다.

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Head_Player"), TEXT("Part_Head"), &headDesc), E_FAIL);

    Head_Player* head = dynamic_cast<Head_Player*>(_partObjects[TEXT("Part_Head")].get());
    CHECK_NULL(head, E_FAIL);

    head->Set_LeaderPoseComponent(body);

    Hair_Player::HAIR_PLAYER_DESC hairDesc{};
    hairDesc.parentMatrix = bodyDesc.parentMatrix;
    hairDesc.modelType = ModelType::Anim;
    hairDesc.modelTag = Get_HairModelTag();
    hairDesc.hairPhysicsDesc = Get_HairChainPhysicsDesc();
    hairDesc.socketMatrixPointer = head->Get_SocketBoneMatrixPtr("head");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Hair_Player"), TEXT("Part_Hair"), &hairDesc), E_FAIL);

    CHECK_FAILED(Ready_WeaponPart(body), E_FAIL);

    return S_OK;
}

HRESULT Player_BattleBase::Ready_WeaponPart(Body_Player* body)
{
    CHECK_NULL(body, E_FAIL);

    WeaponAttachDesc attachDesc{};
    if (!Try_GetWeaponAttackDesc(attachDesc))
        return S_OK; // 무기가 없는 캐릭터는 여기서 끝

    CHECK_NULL(attachDesc.modelTag, E_FAIL);
    CHECK_NULL(attachDesc.socketName, E_FAIL);

    const Matrix* socketMatrix = body->Get_SocketBoneMatrixPtr(attachDesc.socketName);
    CHECK_NULL(socketMatrix, E_FAIL);

    Weapon::WEAPON_DESC weaponDesc;
    weaponDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    weaponDesc.modelType = ModelType::NonAnim;
    weaponDesc.modelTag = attachDesc.modelTag;
    weaponDesc.socketMatrix = socketMatrix;

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static),
            TEXT("GameObject_Weapon"), TEXT("Part_Weapon"), &weaponDesc), E_FAIL);

    return S_OK;

}

HRESULT Player_BattleBase::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_PlayerStat"), _combatStatCom), E_FAIL);

    Bounding_OBB::BoundingOBBDesc obbDesc{};
    obbDesc.center = Vec3(0.f, 0.75f, 0.f);
    obbDesc.extents = Vec3(0.35f, 0.75f, 0.35f);
    CHECK_FAILED(Ready_Collider(L"Com_Collider_OBB", &obbDesc, Collision_Preset::Player_Body), E_FAIL);

    return S_OK;
}
