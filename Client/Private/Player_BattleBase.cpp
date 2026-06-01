#include "Player_BattleBase.h"
#include "Body_Player.h"
#include "Hair_Player.h"
#include "Head_Player.h"
#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "GameInstance.h"
#include "PlayerStatCom.h"
#include "Weapon.h"
#include "EffectCom.h"
#include "WeaponPrefabLoader.h"
#include "BattleMechanismCom.h"
#include "FaceAccessory_Player.h"

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

    _combatStatCom->Set_BattleStartAp();
}

void Player_BattleBase::Update(float timeDelta)
{
    if (_effectCom)
        _effectCom->Update(timeDelta);

    Character_BattleBase::Update(timeDelta);
}

void Player_BattleBase::Late_Update(float timeDelta)
{
    Character_BattleBase::Late_Update(timeDelta);

}

json Player_BattleBase::To_Json() const
{
    json root = GameObject::To_Json();
    root["custom_properties"]["battle_ground_offset_y"] = _battleGroundOffsetY;
    json parts = json::array();

    for (const auto& [partTag, partObject] : _partObjects)
    {
        if (partObject == nullptr)
            continue;

        json partJson = json::object();
        partJson["part_tag"] = String::ToString(partTag);
        partJson["object"] = partObject->To_Json();
        parts.emplace_back(std::move(partJson));
    }

    root["parts"] = std::move(parts);
    return root;
}

void Player_BattleBase::From_Json(const json& data)
{
    Character_BattleBase::From_Json(data);

    _battleGroundOffsetY = 0.f;

    if (data.contains("custom_properties") && data["custom_properties"].is_object())
    {
        const json& customProperties = data["custom_properties"];
        _battleGroundOffsetY = customProperties.value("battle_ground_offset_y", 0.f);
    }
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

    // 수염/콧수염
    if (const wchar_t* beardModelTag = Get_BeardModelTag())
    {
        FaceAccessory_Player::FACE_ACCESSORY_DESC beardPartDesc{};
        beardPartDesc.parentMatrix = bodyDesc.parentMatrix;
        beardPartDesc.modelType = ModelType::NonAnim;
        beardPartDesc.modelTag = beardModelTag;
        beardPartDesc.socketMatrixPointer = head->Get_SocketBoneMatrixPtr("head");

        if (FAILED(
            __super::Add_PartObject(
                ETOI(LevelType::Static),
                TEXT("GameObject_FaceAccessory_Player"),
                TEXT("Part_Beard"),
                &beardPartDesc)
        ))
            return E_FAIL;
    }

    if (const wchar_t* mustacheModelTag = Get_MustacheModelTag())
    {
        FaceAccessory_Player::FACE_ACCESSORY_DESC mustachePartDesc{};
        mustachePartDesc.parentMatrix = bodyDesc.parentMatrix;
        mustachePartDesc.modelType = ModelType::NonAnim;
        mustachePartDesc.modelTag = mustacheModelTag;
        mustachePartDesc.socketMatrixPointer = head->Get_SocketBoneMatrixPtr("head");

        if (FAILED(
            __super::Add_PartObject(
                ETOI(LevelType::Static),
                TEXT("GameObject_FaceAccessory_Player"),
                TEXT("Part_Mustache"),
                &mustachePartDesc)
        ))
            return E_FAIL;
    }

    Hair_Player::HAIR_PLAYER_DESC hairDesc{};
    hairDesc.parentMatrix = bodyDesc.parentMatrix;
    hairDesc.modelType = ModelType::Anim;
    hairDesc.modelTag = Get_HairModelTag();
    hairDesc.hairPhysicsDesc = Get_HairChainPhysicsDesc();
    hairDesc.socketMatrixPointer = head->Get_SocketBoneMatrixPtr("head");

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Hair_Player"), TEXT("Part_Hair"), &hairDesc), E_FAIL);

    CHECK_FAILED(Ready_WeaponParts(body), E_FAIL);

    return S_OK;
}

HRESULT Player_BattleBase::Ready_WeaponParts(Body_Player* body)
{
    CHECK_NULL(body, E_FAIL);

    vector<WeaponAttachDesc> attachDescs{};
    Get_WeaponAttachDescs(attachDescs);

    for (const auto& attachDesc : attachDescs)
    {
        if (attachDesc.partTag.empty())
            return E_FAIL;
        if (attachDesc.weaponPrefabName.empty())
            return E_FAIL;

        WeaponPrefabLoader::LoadedWeaponPrefab loadedPrefab{};
        if (!WeaponPrefabLoader::Load(attachDesc.weaponPrefabName, loadedPrefab))
            return E_FAIL;

        Weapon::WEAPON_DESC weaponDesc{};
        weaponDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
        weaponDesc.modelType = loadedPrefab.modelType;
        weaponDesc.modelTag = loadedPrefab.modelTag.c_str();
        weaponDesc.socketMatrix = nullptr;
        weaponDesc.useBodyFollow = attachDesc.useBodyFollow;
        weaponDesc.bodyFollowOffset = attachDesc.bodyFollowOffset;

        if (attachDesc.useBodyFollow)
        {
            weaponDesc.socketMatrix = nullptr;
        }
        else if (attachDesc.socketName != nullptr)
        {
            const Matrix* socketMatrix = body->Get_SocketBoneMatrixPtr(attachDesc.socketName);
            CHECK_NULL(socketMatrix, E_FAIL);

            weaponDesc.socketMatrix = socketMatrix;
        }
        else
        {
            weaponDesc.socketMatrix = nullptr;
        }

        CHECK_FAILED(Add_PartObject(
            ETOI(LevelType::Static),
            TEXT("GameObject_Weapon"),
            attachDesc.partTag.c_str(),
            &weaponDesc), E_FAIL);

        PartObject* weaponPart = Get_PartObject(attachDesc.partTag);
        CHECK_NULL(weaponPart, E_FAIL);

        weaponPart->From_Json(loadedPrefab.rootJson);
        weaponPart->Set_SourcePrefabName(attachDesc.weaponPrefabName);
    }

    return S_OK;
}

HRESULT Player_BattleBase::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_PlayerStat"), _combatStatCom), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Effect"), _effectCom), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_BattleMechanism"), _battleMechanismCom), E_FAIL);

    Bounding_OBB::BoundingOBBDesc obbDesc{};
    obbDesc.center = Vec3(0.f, 0.75f, 0.f);
    obbDesc.extents = Vec3(0.35f, 0.75f, 0.35f);
    CHECK_FAILED(Ready_Collider(L"Com_Collider_OBB", &obbDesc, Collision_Preset::Player_Body), E_FAIL);

    return S_OK;
}
