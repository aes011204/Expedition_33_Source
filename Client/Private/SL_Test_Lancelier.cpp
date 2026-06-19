#include "SL_Test_Lancelier.h"

#include "Bounding_OBB.h"
#include "ClientInstance.h"
#include "ColliderCom.h"
#include "GameInstance.h"
#include "ModelCom.h"
#include "Mon_Lancelier_Body.h"
#include "Mon_Lancelier_Weapon.h"
#include "MonsterStatCom.h"
#include "SL_DamageSystem.h"
#include "SL_Maelle.h"
#include "SoulLikeController.h"
#include "TransformCom.h"

REGISTER_GLOBAL_ENUM(SL_Test_Lancelier::Anim, "SL_TestLancelierAnim")

IMPLEMENT_REFLECTION(SL_Test_Lancelier)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "SL_Test_Lancelier";
    info.category = "SoulLike";

    return true;
}

SL_Test_Lancelier::SL_Test_Lancelier(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : SL_Base(device, context)
{
}

SL_Test_Lancelier::SL_Test_Lancelier(const SL_Test_Lancelier& prototype)
    : SL_Base(prototype)
{
}

HRESULT SL_Test_Lancelier::Initialize_Prototype()
{
    return SL_Base::Initialize_Prototype();
}

HRESULT SL_Test_Lancelier::Initialize(void* arg)
{
    CHECK_FAILED(SL_Base::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);

    return S_OK;
}

void SL_Test_Lancelier::BeginPlay()
{
    SL_Base::BeginPlay();

    if (_attackColliderCom)
    {
        _attackColliderCom->Set_IsActive(false);
        _attackColliderCom->Clear_Overlap();
    }

    Play_Anim(Anim::Idle, 0.1f, true);
}

void SL_Test_Lancelier::Update(float timeDelta)
{
    if (_combatStat == nullptr || _body == nullptr)
        return;

    Update_Attack(timeDelta);

    _body->Play_Animation(timeDelta);

    SL_Base::Update(timeDelta);
}

void SL_Test_Lancelier::Late_Update(float timeDelta)
{
    SL_Base::Late_Update(timeDelta);

    Update_Culling(Vec3(1.5f, 4.f, 1.5f));
}

HRESULT SL_Test_Lancelier::Render()
{
    return SL_Base::Render();
}

HRESULT SL_Test_Lancelier::Render_Shadow()
{
    return SL_Base::Render_Shadow();
}

void SL_Test_Lancelier::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    SL_Base::OnBeginOverlap(self, other);

    if (self == nullptr || other == nullptr)
        return;

    // 몬스터 공격 collider가 만든 overlap만 Maelle 피격으로 처리한다.
    if (self.get() != _attackColliderCom.get())
        return;

    if (other->Get_CollisionPreset() != Collision_Preset::SL_PlayerBody)
        return;

    const Shared<SL_Base> target = dynamic_pointer_cast<SL_Base>(other->Get_Owner());
    if (target == nullptr || target.get() == this || target->Is_Dead())
        return;

    SL_DamageInfo damageInfo{};
    if (!SL_DamageSystem::Apply_Damage(
        GetSharedPtr<GameObject>(),
        _combatStat,
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

Shared<CombatStatCom> SL_Test_Lancelier::Get_Stat() const
{
    return _combatStat;
}

Shared<ColliderCom> SL_Test_Lancelier::Get_AttackCollider() const
{
    return _attackColliderCom;
}

bool SL_Test_Lancelier::Is_Dead() const
{
    return _combatStat == nullptr || _combatStat->Get_AliveStatus() == EAliveStatus::Dead;
}

void SL_Test_Lancelier::Apply_Damage(const SL_DamageInfo& damageInfo)
{
    if (damageInfo.target.get() != this)
        return;

    Play_Anim(Is_Dead() ? Anim::Dead : Anim::Hit, 0.08f, false);
}

HRESULT SL_Test_Lancelier::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_MonsterStat"), _combatStat), E_FAIL);

    _combatStat->Set_RuntimeCombatProfile(150.f, 0, 18.f, 0.f, 0.f);

    Bounding_OBB::BoundingOBBDesc bodyObbDesc{};
    bodyObbDesc.center = Vec3(0.f, 1.2f, 0.f);
    bodyObbDesc.extents = Vec3(0.65f, 1.2f, 0.45f);

    CHECK_FAILED(Ready_Collider(L"Com_Collider_OBB", &bodyObbDesc, Collision_Preset::SL_MonsterBody), E_FAIL);

    // 같은 prototype tag의 ColliderCom을 하나 더 붙이기 위해 body collider의 component tag를 먼저 분리한다.
    CHECK_FAILED(Rename_ComponentTag(TEXT("Com_Collider_OBB"), TEXT("Com_SL_MonsterBody")), E_FAIL);

    Bounding_OBB::BoundingOBBDesc attackObbDesc{};
    attackObbDesc.center = Vec3(0.f, 1.0f, 1.0f);
    attackObbDesc.extents = Vec3(0.55f, 0.65f, 0.95f);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_OBB"), _attackColliderCom, &attackObbDesc), E_FAIL);
    CHECK_FAILED(Rename_ComponentTag(TEXT("Com_Collider_OBB"), TEXT("Com_SL_MonsterAttack")), E_FAIL);

    // 공격 collider는 ANS_CollisionEnable 구간에서만 활성화한다.
    _attackColliderCom->Set_CollisionPreset(Collision_Preset::SL_MonsterAttack);
    _attackColliderCom->Set_IsActive(false);
    _attackColliderCom->Clear_Overlap();

    return S_OK;
}

HRESULT SL_Test_Lancelier::Ready_PartObjects()
{
    Mon_Lancelier_Body::BODY_DESC bodyDesc{};
    bodyDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    bodyDesc.modelType = ModelType::Anim;
    bodyDesc.modelTag = L"Model_Lancelier";

    CHECK_FAILED(Add_PartObject(
        ETOI(LevelType::Static),
        TEXT("GameObject_Lancelier_Body"),
        TEXT("Part_Body"),
        &bodyDesc), E_FAIL);

    _body = dynamic_pointer_cast<Mon_Lancelier_Body>(_partObjects[TEXT("Part_Body")]);
    CHECK_NULL(_body, E_FAIL);

    if (Shared<ModelCom> model = _body->Get_Model())
        model->Refresh_AnimStateMappingFromOwnerContext();

    Mon_Lancelier_Weapon::WEAPON_DESC weaponDesc{};
    weaponDesc.parentMatrix = &_transformCom->Get_WorldMatrix();
    weaponDesc.modelType = ModelType::NonAnim;
    weaponDesc.modelTag = L"Model_Lancelier_Weapon";
    weaponDesc.socketMatrix = _body->Get_SocketBoneMatrixPtr("Weapon_R");

    CHECK_FAILED(Add_PartObject(
        ETOI(LevelType::Static),
        TEXT("GameObject_Lancelier_Weapon"),
        TEXT("Part_Weapon"),
        &weaponDesc), E_FAIL);

    _weapon = dynamic_pointer_cast<Mon_Lancelier_Weapon>(_partObjects[TEXT("Part_Weapon")]);
    CHECK_NULL(_weapon, E_FAIL);

    return S_OK;
}

void SL_Test_Lancelier::Play_Anim(Anim anim, float blendDuration, bool loop)
{
    if (_body == nullptr)
        return;

    if (_currentAnim == anim && loop)
        return;

    if (_body->Set_AnimationByState(ETOI(anim), blendDuration, loop))
        _currentAnim = anim;
}

void SL_Test_Lancelier::Update_Attack(float timeDelta)
{
    if (Is_Dead())
    {
        Play_Anim(Anim::Dead, 0.1f, false);
        return;
    }

    if (_currentAnim == Anim::Hit && !_body->Is_AnimationFinished())
        return;

    if ((_currentAnim == Anim::Attack_1 || _currentAnim == Anim::Attack_2) && !_body->Is_AnimationFinished())
        return;

    _attackCooldown = max(0.f, _attackCooldown - timeDelta);

    const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController();
    const Shared<SL_Maelle> player = controller ? controller->Get_Player() : nullptr;

    if (player == nullptr || player->Get_Transform() == nullptr || _transformCom == nullptr)
    {
        Play_Anim(Anim::Idle, 0.2f, true);
        return;
    }

    LookAt_Player();

    const float distance = Vec3::Distance(
        _transformCom->Get_WorldPosition(),
        player->Get_Transform()->Get_WorldPosition());

    if (distance <= _attackRange && _attackCooldown <= 0.f)
    {
        _attackCooldown = _attackInterval;
        const Anim attackAnim = _nextAttackAnim;
        _nextAttackAnim = (_nextAttackAnim == Anim::Attack_1) ? Anim::Attack_2 : Anim::Attack_1;
        Play_Anim(attackAnim, 0.1f, false);
        return;
    }

    Play_Anim(Anim::Idle, 0.2f, true);
}

void SL_Test_Lancelier::LookAt_Player()
{
    const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController();
    const Shared<SL_Maelle> player = controller ? controller->Get_Player() : nullptr;

    if (player == nullptr || player->Get_Transform() == nullptr || _transformCom == nullptr)
        return;

    Vec3 lookPos = player->Get_Transform()->Get_WorldPosition();
    lookPos.y = _transformCom->Get_WorldPosition().y;

    _transformCom->LookAt(lookPos);
}

Shared<SL_Test_Lancelier> SL_Test_Lancelier::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<SL_Test_Lancelier>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : SL_Test_Lancelier");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> SL_Test_Lancelier::Clone(void* arg)
{
    auto clone = make_shared<SL_Test_Lancelier>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : SL_Test_Lancelier");
        return nullptr;
    }

    return clone;
}

void SL_Test_Lancelier::Free()
{
    SL_Base::Free();
}
