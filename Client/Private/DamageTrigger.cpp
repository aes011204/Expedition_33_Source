#include "DamageTrigger.h"

#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "CombatStatCom.h"
#include "SL_Base.h"
#include "SL_DamageSystem.h"
#include "SL_Maelle.h"
#include "ClientInstance.h"
#include "SoulLikeController.h"

IMPLEMENT_REFLECTION(DamageTrigger)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "DamageTrigger";
    info.category = "Trigger";

    PROPERTY_FLOAT("Life Time", _lifeTimeSec, 0.01f, 30.f, 0.01f);
    PROPERTY_FLOAT("Damage Multiplier", _damageMultiplier, 0.f, 100.f, 0.1f);
    PROPERTY_ENUM("Attack Type", _attackType, AttackType);
    PROPERTY_ENUM("Hit Reaction", _hitAnimOverride, SL_HitAnimOverride);
    PROPERTY_BOOL("Enable Parry Reaction", _enableParryReaction);

    return true;
}

DamageTrigger::DamageTrigger(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject(device, context)
{
}

DamageTrigger::DamageTrigger(const DamageTrigger& prototype)
    : GameObject(prototype)
{
}

HRESULT DamageTrigger::Initialize_Prototype()
{
    return S_OK;
}

HRESULT DamageTrigger::Initialize(void* arg)
{
    GAMEOBJECT_DESC desc{};
    desc.speedPerSec = 0.f;
    desc.degreePerSec = 0.f;

    CHECK_FAILED(__super::Initialize(&desc), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    _spawnedLevelIndex = GAME->Current_LevelIndex();
    _elapsedSec = 0.f;
    _pendingDestroy = false;
    _lifeTimeSec = max(_lifeTimeSec, 0.01f);
    _damageMultiplier = max(_damageMultiplier, 0.f);

    UNREFERENCED_PARAMETER(arg);
    return S_OK;
}

void DamageTrigger::Update(float timeDelta)
{
    GameObject::Update(timeDelta);

    _elapsedSec += timeDelta;

    if (_pendingDestroy || _elapsedSec >= _lifeTimeSec)
    {
        GAME->Remove_GameObject(_spawnedLevelIndex, GetSharedPtr<GameObject>());
        return;
    }
}

void DamageTrigger::Late_Update(float timeDelta)
{
    GameObject::Late_Update(timeDelta);

    if (_colliderCom == nullptr || !_colliderCom->Get_IsActive())
        return;

    _colliderCom->Update_Collider(_transformCom->Get_WorldMatrix());
    GAME->Add_Collider(_colliderCom);

    UNREFERENCED_PARAMETER(timeDelta);
}

void DamageTrigger::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    GameObject::OnBeginOverlap(self, other);

    if (self == nullptr || other == nullptr)
        return;

    const Shared<SL_Base> attacker = dynamic_pointer_cast<SL_Base>(Get_Owner());
    if (attacker == nullptr)
        return;

    const Shared<CombatStatCom> attackerStat = attacker->Get_Stat();
    if (attackerStat == nullptr)
        return;

    const Collision_Preset expectedTargetPreset = attackerStat->Is_Player()
        ? Collision_Preset::SL_MonsterBody
        : Collision_Preset::SL_PlayerBody;
    if (other->Get_CollisionPreset() != expectedTargetPreset)
        return;

    const Shared<SL_Base> target = dynamic_pointer_cast<SL_Base>(other->Get_Owner());
    if (target == nullptr || target.get() == attacker.get() || target->Is_Dead())
        return;

    if (_enableParryReaction)
    {
        if (const Shared<SL_Maelle> maelle = dynamic_pointer_cast<SL_Maelle>(target))
        {
            const Shared<TransformCom> maelleTransform = maelle->Get_Transform();
            const Vec3 maelleWorldPos = maelleTransform
                ? maelleTransform->Get_WorldPosition()
                : Vec3::Zero;

            const Shared<TransformCom> triggerTransform = Get_Transform();
            const Vec3 triggerWorldPos = triggerTransform
                ? triggerTransform->Get_WorldPosition()
                : maelleWorldPos;

            Vec3 effectDir = triggerWorldPos - maelleWorldPos;
            effectDir.y = 0.f;

            if (effectDir.LengthSquared() > 0.0001f)
                effectDir.Normalize();
            else if (maelleTransform)
                effectDir = maelleTransform->Get_WorldForward();

            effectDir.y = 0.f;
            if (effectDir.LengthSquared() > 0.0001f)
                effectDir.Normalize();

            Vec3 parryEffectWorldPos = maelleWorldPos + effectDir * 0.8f;
            parryEffectWorldPos.y += 1.0f;

            if (maelle->Try_ConsumeParry(attacker, triggerWorldPos, parryEffectWorldPos))
            {
                self->Set_IsActive(false);
                self->Clear_Overlap();
                _pendingDestroy = true;
                return;
            }

            if (const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController())
            {
                if (controller->Try_ConsumeJumpCounterDodge(attacker, maelle))
                {
                    self->Set_IsActive(false);
                    self->Clear_Overlap();
                    _pendingDestroy = true;
                    return;
                }
            }
        }
    }

    SL_DamageInfo damageInfo{};
    if (!SL_DamageSystem::Apply_Damage(
        attacker,
        attackerStat,
        target,
        target->Get_Stat(),
        _damageMultiplier,
        _attackType,
        damageInfo))
    {
        return;
    }

    damageInfo.hitAnimOverride = _hitAnimOverride;
    target->Apply_Damage(damageInfo);

    self->Set_IsActive(false);
    self->Clear_Overlap();
    _pendingDestroy = true;
}

void DamageTrigger::Set_LifeTimeSec(float lifeTimeSec)
{
    _lifeTimeSec = max(lifeTimeSec, 0.01f);
}

void DamageTrigger::Set_EnableParryReaction(bool enableParryReaction)
{
    _enableParryReaction = enableParryReaction;
}

void DamageTrigger::Refresh_CollisionPresetFromOwner()
{
    if (_colliderCom == nullptr)
        return;

    const Shared<SL_Base> attacker = dynamic_pointer_cast<SL_Base>(Get_Owner());
    const Shared<CombatStatCom> attackerStat = attacker ? attacker->Get_Stat() : nullptr;
    if (attackerStat == nullptr)
        return;

    _colliderCom->Set_CollisionPreset(attackerStat->Is_Player()
        ? Collision_Preset::SL_PlayerAttack
        : Collision_Preset::SL_MonsterAttack);
}

HRESULT DamageTrigger::Ready_Components()
{
    Bounding_OBB::BoundingOBBDesc obbDesc{};
    obbDesc.center = Vec3(0.f, 0.7f, 0.8f);
    obbDesc.extents = Vec3(0.45f, 0.45f, 0.9f);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_OBB"), _colliderCom, &obbDesc), E_FAIL);
    _colliderCom->Set_CollisionPreset(Collision_Preset::SL_MonsterAttack);
    _colliderCom->Set_IsActive(true);

    return S_OK;
}

Shared<DamageTrigger> DamageTrigger::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<DamageTrigger>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : DamageTrigger");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> DamageTrigger::Clone(void* arg)
{
    auto clone = make_shared<DamageTrigger>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : DamageTrigger");
        return nullptr;
    }

    return clone;
}

void DamageTrigger::Free()
{
    GameObject::Free();
}
