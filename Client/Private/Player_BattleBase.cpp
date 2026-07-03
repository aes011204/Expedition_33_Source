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
#include "CostumeManager.h"
#include "FaceAccessory_Player.h"
#include "ClientInstance.h"
#include "ModelCom.h"
#include "ChainModifier.h"
#include "ChainModifierPreset.h"
#include "PhysicsModifier.h"
#include "PoseFollowChainModifier.h"

namespace
{
string Resolve_WeaponCharacterId(HeroId heroId)
{
    switch (heroId)
    {
    case HeroId::Gustave:
        return "Gustave";
    case HeroId::Lune:
        return "Lune";
    case HeroId::Maelle:
        return "Maelle";
    case HeroId::END:
    default:
        return {};
    }
}

bool HasBone(const ModelCom* model, const string& boneName)
{
    return model != nullptr && model->Get_BoneIndex(boneName) >= 0;
}

void RemoveMissingGroups(ChainModifier::ChainModifierDesc& desc, const ModelCom* model)
{
    desc.groups.erase(
        remove_if(
            desc.groups.begin(),
            desc.groups.end(),
            [model](const ChainModifier::BoneChainGroupDesc& group)
            {
                return !HasBone(model, group.rootBoneName);
            }),
        desc.groups.end());
}

void RemoveMissingGroups(PoseFollowChainModifier::PoseFollowChainModifierDesc& desc, const ModelCom* model)
{
    desc.groups.erase(
        remove_if(
            desc.groups.begin(),
            desc.groups.end(),
            [model](const PoseFollowChainModifier::BoneChainGroupDesc& group)
            {
                return !HasBone(model, group.rootBoneName);
            }),
        desc.groups.end());
}

void RemoveMissingChains(PhysicsModifier::PhysicsModifierDesc& desc, const ModelCom* model)
{
    desc.chains.erase(
        remove_if(
            desc.chains.begin(),
            desc.chains.end(),
            [model](const PhysicsModifier::ChainDesc& chain)
            {
                return !HasBone(model, chain.rootBoneName);
            }),
        desc.chains.end());
}
}

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

        if (partTag.starts_with(TEXT("Part_Weapon")))
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

    Apply_CurrentCostumePartTransforms();
}

HRESULT Player_BattleBase::Rebuild_WeaponPartsFromCurrentStat()
{
    Body_Player* body = dynamic_cast<Body_Player*>(Get_PartObject(TEXT("Part_Body")));
    CHECK_NULL(body, E_FAIL);

    vector<wstring> weaponPartTags{};
    for (const auto& [partTag, partObject] : _partObjects)
    {
        UNREFERENCED_PARAMETER(partObject);

        if (partTag.starts_with(TEXT("Part_Weapon")))
            weaponPartTags.push_back(partTag);
    }

    for (const wstring& partTag : weaponPartTags)
        Remove_PartObject(partTag);

    // 현재 장착 weaponId 기준으로 WP_* prefab transform을 최종 적용한다.
    CHECK_FAILED(Ready_WeaponParts(body), E_FAIL);
    return S_OK;
}

HRESULT Player_BattleBase::Apply_WeaponPreviewById(const string& weaponId)
{
    if (weaponId.empty())
        return E_FAIL;

    const string characterId = Get_WeaponCharacterId();
    const GameDataTable::WeaponDesc* weaponDesc = CLIENT->Get_WeaponDesc(characterId, weaponId);
    if (weaponDesc == nullptr)
        return E_FAIL;

    _editorPreviewWeaponId = weaponDesc->weaponId;
    return Rebuild_WeaponPartsFromCurrentStat();
}

string Player_BattleBase::Get_WeaponCharacterId() const
{
    return Resolve_WeaponCharacterId(Get_HeroId());
}

string Player_BattleBase::Get_ActiveWeaponPreviewId() const
{
    if (!_editorPreviewWeaponId.empty())
        return _editorPreviewWeaponId;

    return _combatStatCom ? _combatStatCom->Get_PlayerRuntimeStat().CurEquippedWeaponId : string{};
}

HRESULT Player_BattleBase::Ready_PartObjects()
{
    const CostumePreset* costumePreset = Get_CurrentBattleCostume();

    const wchar_t* bodyModelTag = Get_BodyModelTag();
    const wchar_t* headModelTag = Get_HeadModelTag();
    const wchar_t* hairModelTag = Get_HairModelTag();
    const wchar_t* beardModelTag = Get_BeardModelTag();
    const wchar_t* mustacheModelTag = Get_MustacheModelTag();
    const wchar_t* glassesModelTag = nullptr;

    if (costumePreset != nullptr)
    {
        // preset 태그가 비어 있으면 기존 캐릭터 기본 태그를 유지한다.
        bodyModelTag = Select_CostumeModelTag(costumePreset->parts.bodyModelTag, bodyModelTag);
        headModelTag = Select_CostumeModelTag(costumePreset->parts.headModelTag, headModelTag);

        if (costumePreset->parts.hairModelTag.empty())
            hairModelTag = nullptr;
        else
            hairModelTag = costumePreset->parts.hairModelTag.c_str();

        beardModelTag = Select_CostumeModelTag(costumePreset->parts.beardModelTag, beardModelTag);
        mustacheModelTag = Select_CostumeModelTag(costumePreset->parts.mustacheModelTag, mustacheModelTag);
        glassesModelTag = Select_CostumeModelTag(costumePreset->parts.glassesModelTag, nullptr);
    }

    Body_Player::BODY_PLAYER_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = bodyModelTag;

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Body_Player"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    Body_Player* body = dynamic_cast<Body_Player*>(_partObjects[TEXT("Part_Body")].get());
    CHECK_NULL(body, E_FAIL);
    CHECK_FAILED(Ready_BattlePartModifiers(TEXT("Part_Body"), bodyModelTag), E_FAIL);

    Head_Player::HEAD_PLAYER_DESC headDesc{};
    headDesc.parentMatrix = bodyDesc.parentMatrix;
    headDesc.modelType = ModelType::Anim;
    headDesc.modelTag = headModelTag;
    headDesc.socketMatrixPointer = nullptr;

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Head_Player"), TEXT("Part_Head"), &headDesc), E_FAIL);

    Head_Player* head = dynamic_cast<Head_Player*>(_partObjects[TEXT("Part_Head")].get());
    CHECK_NULL(head, E_FAIL);

    head->Set_LeaderPoseComponent(body);

    CHECK_FAILED(Add_BattleFaceAccessory(TEXT("Part_Beard"), beardModelTag, bodyDesc.parentMatrix, head), E_FAIL);
    CHECK_FAILED(Add_BattleFaceAccessory(TEXT("Part_Mustache"), mustacheModelTag, bodyDesc.parentMatrix, head), E_FAIL);

    if (hairModelTag != nullptr && hairModelTag[0] != L'\0')
    {
        Hair_Player::HAIR_PLAYER_DESC hairDesc{};
        hairDesc.parentMatrix = bodyDesc.parentMatrix;
        hairDesc.modelType = ModelType::Anim;
        hairDesc.modelTag = hairModelTag;
        hairDesc.hairPhysicsDesc = Get_HairChainPhysicsDesc();
        hairDesc.socketMatrixPointer = head->Get_SocketBoneMatrixPtr("head");

        CHECK_FAILED(Add_PartObject(
            ETOI(LevelType::Static),
            TEXT("GameObject_Hair_Player"),
            TEXT("Part_Hair"),
            &hairDesc), E_FAIL);

        CHECK_FAILED(Ready_BattlePartModifiers(TEXT("Part_Hair"), hairModelTag), E_FAIL);
    }

    CHECK_FAILED(Add_BattleFaceAccessory(TEXT("Part_Glasses"), glassesModelTag, bodyDesc.parentMatrix, head), E_FAIL);

    // 무기는 옷장에서 변경 ㄴ
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
        weaponDesc.applyAttachScaleCompensation = true; // Battle 장착 weapon은 WP prefab scale 0.1을 실제 크기로 x10 보정한다.

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

bool Player_BattleBase::Should_ApplySerializedPart(const wstring& partTag, const json& partObjectJson) const
{
    UNREFERENCED_PARAMETER(partObjectJson);

    // Battle player costume parts are rebuilt from CostumeManager, not from stale PB prefab part JSON.
    if (partTag == TEXT("Part_Body") ||
        partTag == TEXT("Part_Head") ||
        partTag == TEXT("Part_Hair") ||
        partTag == TEXT("Part_Beard") ||
        partTag == TEXT("Part_Mustache") ||
        partTag == TEXT("Part_Glasses"))
        return false;

    // 프리펩에서 저장한 값이 아니라 PlayerStatCom -> DT Weapon -> WP_* prefab에서 세팅한 값으로 들어가게
    return !partTag.starts_with(TEXT("Part_Weapon"));
}

const CostumePreset* Player_BattleBase::Get_CurrentBattleCostume() const
{
    CostumeManager* costumeManager = CLIENT->Get_CostumeManager();
    if (costumeManager == nullptr)
        return nullptr;

    return costumeManager->Get_CurrentCostume(Get_HeroId());
}

const wchar_t* Player_BattleBase::Select_CostumeModelTag(const wstring& costumeModelTag,
    const wchar_t* defaultModelTag) const
{
    if (!costumeModelTag.empty())
        return costumeModelTag.c_str();

    return defaultModelTag;
}

HRESULT Player_BattleBase::Add_BattleFaceAccessory(const wstring& partTag, const wchar_t* modelTag,
    const Matrix* parentMatrix, Head_Player* head)
{
    // 빈 태그는 "이 파츠 없음"으로 취급
    if (modelTag == nullptr || modelTag[0] == L'\0')
        return S_OK;

    CHECK_NULL(head, E_FAIL);

    FaceAccessory_Player::FACE_ACCESSORY_DESC accessoryDesc{};
    accessoryDesc.parentMatrix = parentMatrix;
    accessoryDesc.modelType = ModelType::NonAnim;
    accessoryDesc.modelTag = modelTag;
    accessoryDesc.socketMatrixPointer = head->Get_SocketBoneMatrixPtr("head");

    CHECK_FAILED(Add_PartObject(ETOI
    (LevelType::Static), TEXT("GameObject_FaceAccessory_Player"), partTag.c_str(), &accessoryDesc), E_FAIL);

    return S_OK;
}

HRESULT Player_BattleBase::Add_BodyArmbandAndCoatModifiers(Body_Player* body, PlayerCharacterType characterType)
{
    CHECK_NULL(body, E_FAIL);
    CHECK_NULL(Get_Transform(), E_FAIL);

    const Shared<ModelCom> model = body->Get_Model();
    CHECK_NULL(model, E_FAIL);

    ChainModifier::ChainModifierDesc armbandDesc{};
    ChainModifierPreset::BuildArmbandDesc(
        armbandDesc,
        Get_Transform().get(),
        characterType);

    if (model->Find_AnimModifierByTag(armbandDesc.modifierTag) == nullptr)
    {
        RemoveMissingGroups(armbandDesc, model.get());
        if (!armbandDesc.groups.empty())
        {
            if (auto modifier = ChainModifier::Create(&armbandDesc))
                model->Add_AnimModifier(modifier);
            else
                return E_FAIL;
        }
    }

    PoseFollowChainModifier::PoseFollowChainModifierDesc coatDesc{};
    ChainModifierPreset::BuildPoseFollowCoatDesc(
        coatDesc,
        Get_Transform().get(),
        characterType);

    if (model->Find_AnimModifierByTag(coatDesc.modifierTag) == nullptr)
    {
        RemoveMissingGroups(coatDesc, model.get());
        if (!coatDesc.groups.empty())
        {
            if (auto modifier = PoseFollowChainModifier::Create(&coatDesc))
                model->Add_AnimModifier(modifier);
            else
                return E_FAIL;
        }
    }

    return S_OK;
}

HRESULT Player_BattleBase::Add_GustavePotatoCapeModifier(Body_Player* body)
{
    CHECK_NULL(body, E_FAIL);
    CHECK_NULL(Get_Transform(), E_FAIL);

    const Shared<ModelCom> model = body->Get_Model();
    CHECK_NULL(model, E_FAIL);

    PhysicsModifier::PhysicsModifierDesc potatoCapeDesc{};
    ChainModifierPreset::BuildGustavePotatoCapePhysicsDesc(
        potatoCapeDesc,
        Get_Transform().get());

    if (model->Find_AnimModifierByTag(potatoCapeDesc.modifierTag) != nullptr)
        return S_OK;

    RemoveMissingChains(potatoCapeDesc, model.get());
    if (potatoCapeDesc.chains.empty())
        return S_OK;

    const Matrix* bodyWorldMatrix = &body->Get_CombinedWorldMatrix();
    potatoCapeDesc.ownerWorldMatrix = bodyWorldMatrix;
    potatoCapeDesc.collisionModel = model.get();
    potatoCapeDesc.collisionOwnerWorldMatrix = bodyWorldMatrix;

    if (auto modifier = PhysicsModifier::Create(&potatoCapeDesc))
    {
        model->Add_AnimModifier(modifier);
        return S_OK;
    }

    return E_FAIL;
}

HRESULT Player_BattleBase::Ready_BattlePartModifiers(const wstring& partTag, const wchar_t* modelTag)
{
    PartObject* part = Get_PartObject(partTag);

    if (partTag == TEXT("Part_Body"))
        return Ready_BodyModifiers(
            dynamic_cast<Body_Player*>(part),
            modelTag);

    if (partTag == TEXT("Part_Hair"))
        return Ready_HairModifiers(
            dynamic_cast<Hair_Player*>(part),
            modelTag);

    return S_OK;
}

void Player_BattleBase::Apply_CurrentCostumePartTransforms()
{
    const CostumePreset* costumePreset = Get_CurrentBattleCostume();
    if (costumePreset == nullptr)
        return;

    Apply_CostumePartTransform(TEXT("Part_Body"), costumePreset->parts.bodyTransform);
    Apply_CostumePartTransform(TEXT("Part_Head"), costumePreset->parts.headTransform);
    Apply_CostumePartTransform(TEXT("Part_Beard"), costumePreset->parts.beardTransform);
    Apply_CostumePartTransform(TEXT("Part_Mustache"), costumePreset->parts.mustacheTransform);
    Apply_CostumePartTransform(TEXT("Part_Hair"), costumePreset->parts.hairTransform);
    Apply_CostumePartTransform(TEXT("Part_Glasses"), costumePreset->parts.glassesTransform);
}

void Player_BattleBase::Apply_CostumePartTransform(const wstring& partTag,
    const CostumePartLocalTransform& fallbackTransform)
{
    if (!fallbackTransform.enabled)
        return;

    PartObject* partObject = Get_PartObject(partTag);
    if (partObject == nullptr || partObject->Get_Transform() == nullptr)
        return;

    partObject->Get_Transform()->Set_Position(fallbackTransform.position);
    partObject->Get_Transform()->Set_RotationEuler(fallbackTransform.rotation);
    partObject->Get_Transform()->Set_Scale(fallbackTransform.scale);
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
