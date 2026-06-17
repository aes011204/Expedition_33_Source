#include "SL_Maelle.h"

#include "Body_Player.h"
#include "Bounding_OBB.h"
#include "ClientInstance.h"
#include "CostumeManager.h"
#include "EffectCom.h"
#include "MovementCom.h"
#include "ColliderCom.h"
#include "Hair_Player.h"
#include "Head_Player.h"
#include "PartObject.h"

REGISTER_GLOBAL_ENUM(Client::SL_Maelle::SoulLikeAnim, "SL_MaelleAnim")

IMPLEMENT_REFLECTION(SL_Maelle)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "SL_Maelle";
    info.category = "Character";

    return true;
}

SL_Maelle::SL_Maelle(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Character_WorldBase(device, context)
{
}

SL_Maelle::SL_Maelle(const SL_Maelle& prototype)
    : Character_WorldBase(prototype)
{
}

HRESULT SL_Maelle::Initialize_Prototype()
{
    return Character_WorldBase::Initialize_Prototype();
}

HRESULT SL_Maelle::Initialize(void* arg)
{
    Character_WorldBase::ContainerObjectDesc desc{};
    desc.speedPerSec = 20.f;
    desc.degreePerSec = 720.f;

    CHECK_FAILED(Character_WorldBase::Initialize(&desc), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    return S_OK;
}

void SL_Maelle::BeginPlay()
{
    Character_WorldBase::BeginPlay();

    if (_movementCom)
        _movementCom->Sync_RuntimePositionToTransform();

    Play_SoulLikeAnim(SoulLikeAnim::Idle, 0.1f, true);
}

void SL_Maelle::Priority_Update(float timeDelta)
{
    Character_WorldBase::Priority_Update(timeDelta);
}

void SL_Maelle::Update(float timeDelta)
{
    Character_WorldBase::Update(timeDelta);

    _invincibleRemain = max(0.f, _invincibleRemain - timeDelta);

    if (_effectCom)
        _effectCom->Update(timeDelta);
}

void SL_Maelle::Late_Update(float timeDelta)
{
    Character_WorldBase::Late_Update(timeDelta);

    Update_Culling(Vec3(1.f, 2.f, 1.f));
}

HRESULT SL_Maelle::Render()
{
    return Character_WorldBase::Render();
}

HRESULT SL_Maelle::Render_Shadow()
{
    return Character_WorldBase::Render_Shadow();
}

void SL_Maelle::Reset_CombatHp(float maxHp)
{
    _hp = max(1.f, maxHp);
    _invincibleRemain = 0.f;
    _combatLocked = false;
    Play_SoulLikeAnim(SoulLikeAnim::Idle, 0.1f, true);
}

void SL_Maelle::Clear_CombatLockIfAnimFinished()
{
    if (!_combatLocked)
        return;

    if (!Is_AnimFinished())
        return;

    _combatLocked = false;
    Play_SoulLikeAnim(SoulLikeAnim::Idle, 0.1f, true);
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

    Bounding_OBB::BoundingOBBDesc obbDesc{};
    obbDesc.center = Vec3(0.f, 0.75f, 0.f);
    obbDesc.extents = Vec3(0.35f, 0.75f, 0.35f);

    CHECK_FAILED(Ready_Collider(L"Com_Collider_OBB", &obbDesc, Collision_Preset::Player_WorldBody), E_FAIL);

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
        model->Refresh_AnimStateMappingFromOwnerContext(); // SL_Maelle.prefab 컨텍스트로 SL 전용 animmap을 다시 찾는다.

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

void SL_Maelle::Apply_MoveInput(const Vec3& worldDir, bool sprint, float timeDelta)
{
    if (_movementCom == nullptr)
        return;

    MovementCom::MovementInput input{};
    if (!_combatLocked && worldDir.LengthSquared() > 0.f)
    {
        input.forcedDirection = worldDir;
        input.moveZ = 1.f; // forcedDirection을 실제 이동 방향으로 소비하게 한다.
        input.isRunPressed = sprint;
    }

    _movementCom->Apply_Input(input);
    _movementCom->Update_Movement(timeDelta);

    if (!_combatLocked)
        Play_SoulLikeAnim(worldDir.LengthSquared() > 0.f ? SoulLikeAnim::Move : SoulLikeAnim::Idle, 0.15f, true);
}

void SL_Maelle::Stop_Move()
{
    if (_movementCom)
    {
        _movementCom->Apply_Input({});
        _movementCom->Stop_AllMovement();
    }
}

bool SL_Maelle::Play_SoulLikeAnim(SoulLikeAnim anim, float blendDuration, bool loop)
{
    if (_body == nullptr)
        return false;

    return _body->Set_AnimationByState(ETOI(anim), blendDuration, loop);
}

bool SL_Maelle::Is_AnimFinished() const
{
    return _body && _body->Is_AnimationFinished();
}

void SL_Maelle::Apply_Damage(float damage)
{
    if (Is_Invincible() || Is_Dead())
        return;

    _hp = max(0.f, _hp - damage);
    _combatLocked = true;
    Play_SoulLikeAnim(Is_Dead() ? SoulLikeAnim::Dead : SoulLikeAnim::Hit, 0.1f, false);
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
    Character_WorldBase::Free();
}
