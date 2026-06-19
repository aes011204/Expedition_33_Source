#include "SL_Maelle.h"

#include "Body_Player.h"
#include "Bounding_OBB.h"
#include "Client_Constants.h"
#include "ClientInstance.h"
#include "CostumeManager.h"
#include "EffectCom.h"
#include "GameDataTable.h"
#include "MovementCom.h"
#include "ColliderCom.h"
#include "Hair_Player.h"
#include "Head_Player.h"
#include "ModelCom.h"
#include "PartObject.h"
#include "Weapon.h"
#include "WeaponPrefabLoader.h"
#include "SL_StateMachineCom.h"
#include "SL_TargetCom.h"
#include "SL_Types.h"

#include "SL_DamageSystem.h"

REGISTER_GLOBAL_ENUM(SoulLikeAnim, "SL_MaelleAnim")

IMPLEMENT_REFLECTION(SL_Maelle)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "SL_Maelle";
    info.category = "Character";

    return true;
}

SL_Maelle::SL_Maelle(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : SL_Base(device, context)
{
}

SL_Maelle::SL_Maelle(const SL_Maelle& prototype)
    : SL_Base(prototype)
{
}

HRESULT SL_Maelle::Initialize_Prototype()
{
    return SL_Base::Initialize_Prototype();
}

HRESULT SL_Maelle::Initialize(void* arg)
{
    SL_Base::ContainerObjectDesc desc{};
    desc.speedPerSec = 20.f;
    desc.degreePerSec = 720.f;

    CHECK_FAILED(SL_Base::Initialize(&desc), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    return S_OK;
}

void SL_Maelle::BeginPlay()
{
    SL_Base::BeginPlay();

    if (_movementCom)
        _movementCom->Sync_RuntimePositionToTransform();

    if (_stateMachineCom)
        _stateMachineCom->Change_State(SL_PlayerState::Idle);

}

void SL_Maelle::Priority_Update(float timeDelta)
{
    SL_Base::Priority_Update(timeDelta);
}

void SL_Maelle::Update(float timeDelta)
{
    SL_Base::Update(timeDelta);

    // 무적 시간
    _invincibleRemain = max(0.f, _invincibleRemain - timeDelta);

    if (_targetCom)
        _targetCom->Update_Targeting(timeDelta);

    if (_stateMachineCom)
        _stateMachineCom->Update_CurrentState(timeDelta);

    if (_effectCom)
        _effectCom->Update(timeDelta);
}

void SL_Maelle::Late_Update(float timeDelta)
{
    SL_Base::Late_Update(timeDelta);

    Update_Culling(Vec3(1.f, 2.f, 1.f));
}

HRESULT SL_Maelle::Render()
{
    return SL_Base::Render();
}

HRESULT SL_Maelle::Render_Shadow()
{
    return SL_Base::Render_Shadow();
}

void SL_Maelle::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    SL_Base::OnBeginOverlap(self, other);

    if (self == nullptr || other == nullptr)
        return;

    // Maelle이 가진 공격 collider가 시작한 overlap만 데미지로 처리한다.
    if (self.get() != _attackColliderCom.get())
        return;

    if (other->Get_CollisionPreset() != Collision_Preset::SL_MonsterBody)
        return;

    const Shared<GameObject> otherOwner = other->Get_Owner();
    const Shared<SL_Base> target = dynamic_pointer_cast<SL_Base>(otherOwner);
    if (target == nullptr || target.get() == this || target->Is_Dead())
        return;

    SL_DamageInfo damageInfo{};
    if (!SL_DamageSystem::Apply_Damage(
        GetSharedPtr<GameObject>(),
        _combatStatCom,
        target,
        target->Get_Stat(),
        1.f,
        AttackType::BaseAttack,
        damageInfo))
    {
        return;
    }

    target->Apply_Damage(damageInfo);
}

void SL_Maelle::Reset_CombatHp(float maxHp)
{
    if (_combatStatCom)
    {
        auto& runtimeStat = _combatStatCom->Get_RuntimeStat();
        runtimeStat.CurHp = max(1.f, maxHp);
        runtimeStat.CurAliveState = EAliveStatus::Alive;
    }

    _invincibleRemain = 0.f;
    _combatLocked = false;
}

HRESULT SL_Maelle::Ready_Components()
{
    MovementCom::MoveDesc moveDesc{};
    moveDesc.walkSpeed = 4.f;
    moveDesc.runSpeed = 7.f;
    moveDesc.sprintSpeed = 9.f;
    moveDesc.turnSpeedDegree = 900.f;
    moveDesc.turnResponse = 18.f;
    moveDesc.jumpVelocity = 0.f;
    moveDesc.gravity = -40.f;
    moveDesc.backend = MovementCom::MoveBackend::PhysXCCT;
    moveDesc.cct.radius = 0.32f;
    moveDesc.cct.height = 1.50f;
    moveDesc.cct.stepOffset = 0.35f;
    moveDesc.cct.contactOffset = 0.05f;
    moveDesc.cct.slopeLimitDeg = 55.f;

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Movement"), _movementCom, &moveDesc), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Effect"), _effectCom), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_PlayerStat"), _combatStatCom), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_SL_StateMachine"), _stateMachineCom), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_SL_Target"), _targetCom), E_FAIL);

    // 몸통
    {
        Bounding_OBB::BoundingOBBDesc obbDesc{};
        obbDesc.center = Vec3(0.f, 0.75f, 0.f);
        obbDesc.extents = Vec3(0.35f, 0.75f, 0.35f);

        CHECK_FAILED(Ready_Collider(L"Com_Collider_OBB", &obbDesc, Collision_Preset::SL_PlayerBody), E_FAIL);
        CHECK_FAILED(Rename_ComponentTag(TEXT("Com_Collider_OBB"), TEXT("Com_SL_Body_Player")), E_FAIL);
    }

    // 무기
    {
        Bounding_OBB::BoundingOBBDesc attackObbDesc{};
        attackObbDesc.center = Vec3(0.f, 1.0f, 0.95f);
        attackObbDesc.extents = Vec3(0.55f, 0.65f, 0.85f);

        CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_OBB"), _attackColliderCom, &attackObbDesc), E_FAIL);
        CHECK_FAILED(Rename_ComponentTag(TEXT("Com_Collider_OBB"), TEXT("Com_SL_PlayerAttack")), E_FAIL);

        _attackColliderCom->Set_CollisionPreset(Collision_Preset::SL_PlayerAttack);
        _attackColliderCom->Set_IsActive(false);
        _attackColliderCom->Clear_Overlap();
    }


    return S_OK;
}

HRESULT SL_Maelle::Ready_PartObjects()
{
    const CostumePreset* costumePreset = Get_CurrentSoulLikeCostume();

    const wchar_t* bodyModelTag = L"Model_MaelleBody";
    const wchar_t* headModelTag = L"Model_MaelleHead";
    const wchar_t* hairModelTag = L"Model_MaelleHair";

    if (costumePreset != nullptr)
    {
        bodyModelTag = Select_CostumeModelTag(costumePreset->parts.bodyModelTag, bodyModelTag);
        headModelTag = Select_CostumeModelTag(costumePreset->parts.headModelTag, headModelTag);

        // 빈 hair 태그는 Potato처럼 hair 파츠가 없는 코스튬으로 취급한다.
        hairModelTag = costumePreset->parts.hairModelTag.empty()
            ? nullptr
            : costumePreset->parts.hairModelTag.c_str();
    }

    Body_Player::BODY_PLAYER_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = bodyModelTag;

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Body_Player"), TEXT("Part_Body"), &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<Body_Player>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);

    if (Shared<ModelCom> model = _body->Get_Model())
        model->Refresh_AnimStateMappingFromOwnerContext(); // 초기 파츠 생성 직후 매핑을 잡고, Spawn_Prefab에서 source_prefab 세팅 후 한 번 더 갱신한다.

    Head_Player::HEAD_PLAYER_DESC headDesc{};
    headDesc.parentMatrix = bodyDesc.parentMatrix;
    headDesc.modelType = ModelType::Anim;
    headDesc.modelTag = headModelTag;
    headDesc.socketMatrixPointer = nullptr;

    CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Head_Player"), TEXT("Part_Head"), &headDesc), E_FAIL);

    _head = dynamic_pointer_cast<Head_Player>(_partObjects[TEXT("Part_Head")]);
    CHECK_NULL(_head, E_FAIL);
    _head->Set_LeaderPoseComponent(_body.get());

    if (hairModelTag != nullptr && hairModelTag[0] != L'\0')
    {
        Hair_Player::HAIR_PLAYER_DESC hairDesc{};
        hairDesc.parentMatrix = bodyDesc.parentMatrix;
        hairDesc.modelType = ModelType::Anim;
        hairDesc.modelTag = hairModelTag;
        hairDesc.hairPhysicsDesc = Get_MaelleHairChainPhysicsDesc();
        hairDesc.socketMatrixPointer = _head->Get_SocketBoneMatrixPtr("head");

        CHECK_FAILED(Add_PartObject(ETOI(LevelType::Static), TEXT("GameObject_Hair_Player"), TEXT("Part_Hair"), &hairDesc), E_FAIL);

        _hair = dynamic_pointer_cast<Hair_Player>(_partObjects[TEXT("Part_Hair")]);
        CHECK_NULL(_hair, E_FAIL);
    }

    if (costumePreset != nullptr)
    {
        Apply_CostumePartTransform(TEXT("Part_Body"), costumePreset->parts.bodyTransform);
        Apply_CostumePartTransform(TEXT("Part_Head"), costumePreset->parts.headTransform);
        Apply_CostumePartTransform(TEXT("Part_Hair"), costumePreset->parts.hairTransform);
    }

    CHECK_FAILED(Rebuild_WeaponPartFromCurrentStat(), E_FAIL);

    return S_OK;
}

HRESULT SL_Maelle::Apply_WorldStatSnapshot(const PlayerStatCom::BattleSyncSnapshot& snapshot)
{
    CHECK_NULL(_combatStatCom, E_FAIL);

    _combatStatCom->Apply_BattleSyncSnapshot(snapshot);
    CHECK_FAILED(Rebuild_WeaponPartFromCurrentStat(), E_FAIL);

    return S_OK;
}

void SL_Maelle::Apply_WorldSkillKeys(const vector<string>& skillKeys)
{
    if (_combatStatCom)
    {
        _combatStatCom->Set_SkillKeys(skillKeys);
    }
}

bool SL_Maelle::Try_GetEquippedSkill(uint32 slotIndex, Skill_Data& outSkillData) const
{
    if (_combatStatCom == nullptr)
        return false;

    string skillKey{};
    if (!_combatStatCom->Try_GetSkillKeyBySlot(slotIndex, skillKey))
        return false;

    const Skill_Data* skillData = CLIENT->Get_Skill(skillKey);
    if (skillData == nullptr)
        return false;

    outSkillData = *skillData;
    return true;
}

HRESULT SL_Maelle::Rebuild_WeaponPartFromCurrentStat()
{
    Body_Player* body = dynamic_cast<Body_Player*>(Get_PartObject(TEXT("Part_Body")));
    CHECK_NULL(body, E_FAIL);

    Remove_PartObject(TEXT("Part_Weapon"));

    CHECK_FAILED(Ready_WeaponPart(body), E_FAIL);
    return S_OK;
}

HRESULT SL_Maelle::Ready_WeaponPart(Body_Player* body)
{
    CHECK_NULL(body, E_FAIL);

    const string equippedWeaponId = _combatStatCom
        ? _combatStatCom->Get_PlayerRuntimeStat().CurEquippedWeaponId
        : "";

    const GameDataTable::WeaponDesc* weapon = equippedWeaponId.empty()
        ? nullptr
        : CLIENT->Get_WeaponDesc("Maelle", equippedWeaponId);

    const vector<GameDataTable::WeaponDesc>* weapons = CLIENT->Get_WeaponDescs("Maelle");
    if (weapon == nullptr && weapons != nullptr && !weapons->empty())
        weapon = &weapons->front();

    const string weaponPrefabName = (weapon != nullptr && !weapon->weaponPrefabName.empty())
        ? weapon->weaponPrefabName
        : Constants::Prefab::Maelle_Medalum;

    WeaponPrefabLoader::LoadedWeaponPrefab loadedPrefab{};
    if (!WeaponPrefabLoader::Load(weaponPrefabName, loadedPrefab))
        return E_FAIL;

    Weapon::WEAPON_DESC weaponDesc{};
    weaponDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    weaponDesc.modelType = loadedPrefab.modelType;
    weaponDesc.modelTag = loadedPrefab.modelTag.c_str();
    weaponDesc.socketMatrix = body->Get_SocketBoneMatrixPtr("Weapon_r");
    weaponDesc.applyAttachScaleCompensation = true;

    CHECK_NULL(weaponDesc.socketMatrix, E_FAIL);

    CHECK_FAILED(Add_PartObject(
        ETOI(LevelType::Static),
        TEXT("GameObject_Weapon"),
        TEXT("Part_Weapon"),
        &weaponDesc), E_FAIL);

    PartObject* weaponPart = Get_PartObject(TEXT("Part_Weapon"));
    CHECK_NULL(weaponPart, E_FAIL);

    weaponPart->From_Json(loadedPrefab.rootJson);
    weaponPart->Set_SourcePrefabName(weaponPrefabName);

    return S_OK;
}

const CostumePreset* SL_Maelle::Get_CurrentSoulLikeCostume() const
{
    CostumeManager* costumeManager = CLIENT->Get_CostumeManager();
    if (costumeManager == nullptr)
        return nullptr;

    return costumeManager->Get_CurrentCostume(HeroId::Maelle);
}

const wchar_t* SL_Maelle::Select_CostumeModelTag(const wstring& costumeModelTag, const wchar_t* defaultModelTag) const
{
    if (!costumeModelTag.empty())
        return costumeModelTag.c_str();

    return defaultModelTag;
}

void SL_Maelle::Apply_CostumePartTransform(const wstring& partTag, const CostumePartLocalTransform& fallbackTransform)
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

bool SL_Maelle::Should_ApplySerializedPart(const wstring& partTag, const json& partObjectJson) const
{
    UNREFERENCED_PARAMETER(partObjectJson);

    // SL_Maelle 파츠 배치는 현재 CostumeManager preset이 기준이다.
    // prefab에 저장된 Part_* TransformCom이 나중에 From_Json으로 들어오면 hair offset이 0으로 덮인다.
    if (partTag == TEXT("Part_Body") ||
        partTag == TEXT("Part_Head") ||
        partTag == TEXT("Part_Hair"))
        return false;

    if (partTag.starts_with(TEXT("Part_Weapon")))
        return false;

    return true;
}

HairChainPhysicsDesc SL_Maelle::Get_MaelleHairChainPhysicsDesc() const
{
    HairChainPhysicsDesc desc{};

    desc.profile = HairPhysicsProfile::Maelle;
    desc.bonePrefix = "Strand_";

    desc.stiffness = 0.035f;
    desc.damping = 0.38f;
    desc.windScale = 0.12f;
    desc.gravity = Vec3(0.f, -0.35f, 0.f);

    desc.movementInertiaScale = 0.12f;
    desc.maxMovementInertia = 1.6f;
    desc.maxMovementOffset = 0.045f;

    desc.poseRestoreStrength = 0.15f;
    desc.maxBendAngleDegree = 34.f;

    desc.teleportDistance = 300.f;
    desc.solverIterations = 3;

    return desc;
}

void SL_Maelle::Apply_MoveInput(const SL_MoveRequest& request, float timeDelta)
{
    if (_movementCom == nullptr)
        return;

    MovementCom::MovementInput input{};

    if (!_combatLocked && request.worldDir.LengthSquared() > 0.f)
    {
        input.moveX = request.moveX;
        input.moveZ = request.moveZ;
        input.isRunPressed = request.sprint;
    }

    _movementCom->Apply_Input(input);
    _movementCom->Update_Movement(timeDelta);
}

void SL_Maelle::Stop_Move()
{
    if (_movementCom)
    {
        _movementCom->Apply_Input({});
        _movementCom->Stop_AllMovement();
    }
}

Shared<CombatStatCom> SL_Maelle::Get_Stat() const
{
    return _combatStatCom;
}

Shared<ColliderCom> SL_Maelle::Get_AttackCollider() const
{
    return _attackColliderCom;
}

bool SL_Maelle::Is_Dead() const
{
    if (_combatStatCom == nullptr)
        return true;

    return _combatStatCom->Get_AliveStatus() == EAliveStatus::Dead;
}

void SL_Maelle::Apply_Damage(const SL_DamageInfo& damageInfo)
{
    if (Is_Invincible() || Is_Dead())
        return;

    if (damageInfo.target.get() != this)
        return;

    _combatLocked = true;

    if (_stateMachineCom)
        _stateMachineCom->Change_State(damageInfo.isKilled ? SL_PlayerState::Dead : SL_PlayerState::Hit);
}

Shared<SL_Maelle> SL_Maelle::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<SL_Maelle>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> SL_Maelle::Clone(void* arg)
{
    auto clone = make_shared<SL_Maelle>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void SL_Maelle::Free()
{
    SL_Base::Free();
}
