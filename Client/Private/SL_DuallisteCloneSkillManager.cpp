#include "SL_DuallisteCloneSkillManager.h"

#include "Body_Player.h"
#include "ClientInstance.h"
#include "ModelCom.h"
#include "Player_BattleBase.h"
#include "SL_Base.h"
#include "SL_DamageSystem.h"
#include "SL_Dualliste.h"
#include "Spawn_Prefab.h"
#include "TransformCom.h"

namespace
{
    constexpr float kCloneBaseDamage = 100.f;
}

HRESULT SL_DuallisteCloneSkillManager::Initialize(const Shared<SL_Dualliste>& owner)
{
    CHECK_NULL(owner, E_FAIL);

    _owner = owner;
    _activeClone.reset();

    return S_OK;
}

void SL_DuallisteCloneSkillManager::Update(float timeDelta)
{
    Update_ActiveClone(timeDelta);
}

void SL_DuallisteCloneSkillManager::Reset()
{
    Finish_ActiveClone();
    _activeClone.reset();
}

bool SL_DuallisteCloneSkillManager::Request_CloneSkill(
    const string& prefabName,
    const string& skillKey,
    const string& animationName,
    const Shared<SL_Base>& target,
    float fallbackDamageMultiplier,
    float animationSpeed)
{
    if (_activeClone.has_value())
        return false;

    if (prefabName.empty() || animationName.empty() || target == nullptr || target->Is_Dead())
        return false;

    CloneSkillRuntime runtime{};
    runtime.target = target;
    runtime.levelIndex = GAME->Current_LevelIndex();
    runtime.prefabName = prefabName;
    runtime.skillKey = skillKey;
    runtime.animationName = animationName;
    runtime.fallbackDamageMultiplier = fallbackDamageMultiplier;
    runtime.animationSpeed = animationSpeed;

    return Begin_CloneSkill(std::move(runtime));
}

bool SL_DuallisteCloneSkillManager::Notify_Attack(GameObject* notifyOwner)
{
    if (!_activeClone.has_value())
        return false;

    if (!Is_NotifyOwnerActiveClone(notifyOwner))
        return false;

    CloneSkillRuntime& runtime = *_activeClone;
    if (!runtime.attackApplied)
    {
        Apply_CloneDamage(runtime);
        runtime.attackApplied = true;
    }

    return true;
}

bool SL_DuallisteCloneSkillManager::Begin_CloneSkill(CloneSkillRuntime runtime)
{
    Shared<Player_BattleBase> actor = Spawn_CloneActor(runtime);
    CHECK_NULL(actor, false);

    runtime.actor = actor;

    if (!Play_CloneAnimation(actor, runtime.animationName, runtime.animationSpeed))
    {
        GAME->Remove_GameObject(runtime.levelIndex, actor);
        return false;
    }

    _activeClone = std::move(runtime);
    return true;
}

void SL_DuallisteCloneSkillManager::Update_ActiveClone(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!_activeClone.has_value())
        return;

    Shared<Player_BattleBase> actor = _activeClone->actor.lock();
    if (actor == nullptr)
    {
        _activeClone.reset();
        return;
    }

    Body_Player* body = dynamic_cast<Body_Player*>(actor->Get_PartObject(L"Part_Body"));
    Shared<ModelCom> model = body ? body->Get_Model() : nullptr;
    if (model != nullptr && !model->Is_AnimationFinished())
        return;

    Finish_ActiveClone();
    _activeClone.reset();
}

void SL_DuallisteCloneSkillManager::Finish_ActiveClone()
{
    if (!_activeClone.has_value())
        return;

    if (Shared<Player_BattleBase> actor = _activeClone->actor.lock())
        GAME->Remove_GameObject(_activeClone->levelIndex, actor);
}

Shared<Player_BattleBase> SL_DuallisteCloneSkillManager::Spawn_CloneActor(const CloneSkillRuntime& runtime)
{
    Shared<SL_Dualliste> owner = _owner.lock();
    Shared<SL_Base> target = runtime.target.lock();
    if (owner == nullptr || target == nullptr)
        return nullptr;

    const Vec3 spawnPosition = Resolve_SpawnPosition(owner, target);
    const Vec3 spawnRotation = Resolve_SpawnRotation(spawnPosition, target);

    Shared<GameObject> object = Spawn_Prefab::Prefab(runtime.prefabName)
        .AtLevel(runtime.levelIndex)
        .InLayer(TEXT("Layer_Player"))
        .Position(spawnPosition)
        .Rotation(spawnRotation)
        .Spawn();

    return dynamic_pointer_cast<Player_BattleBase>(object);
}

bool SL_DuallisteCloneSkillManager::Play_CloneAnimation(
    const Shared<Player_BattleBase>& actor,
    const string& animationName,
    float animationSpeed)
{
    if (actor == nullptr)
        return false;

    Body_Player* body = dynamic_cast<Body_Player*>(actor->Get_PartObject(L"Part_Body"));
    Shared<ModelCom> model = body ? body->Get_Model() : nullptr;
    if (model == nullptr)
        return false;

    if (model->Get_AnimationIndexByName(animationName) == UINT_MAX)
        return false;

    model->Set_AnimationSpeedByName(animationName, max(0.01f, animationSpeed));
    model->Set_Animation(animationName, 0.05f, false);

    return true;
}

bool SL_DuallisteCloneSkillManager::Is_NotifyOwnerActiveClone(GameObject* notifyOwner) const
{
    if (!_activeClone.has_value() || notifyOwner == nullptr)
        return false;

    Shared<Player_BattleBase> actor = _activeClone->actor.lock();
    if (actor == nullptr)
        return false;

    Shared<GameObject> current = notifyOwner->GetSharedPtr<GameObject>();
    while (current != nullptr)
    {
        if (current.get() == actor.get())
            return true;

        current = current->Get_Owner();
    }

    return false;
}

void SL_DuallisteCloneSkillManager::Apply_CloneDamage(const CloneSkillRuntime& runtime)
{
    Shared<Player_BattleBase> actor = runtime.actor.lock();
    Shared<SL_Base> target = runtime.target.lock();
    if (actor == nullptr || target == nullptr || target->Is_Dead())
        return;

    const Skill_Data* skillData = runtime.skillKey.empty() ? nullptr : CLIENT->Get_Skill(runtime.skillKey);
    const float multiplier = skillData ? skillData->powerMultiplier : runtime.fallbackDamageMultiplier;

    SL_DamageInfo damageInfo{};
    if (!SL_DamageSystem::Apply_FixedDamage(
        actor,
        target,
        target->Get_Stat(),
        kCloneBaseDamage * multiplier,
        AttackType::SingleSkill,
        damageInfo))
    {
        return;
    }

    target->Apply_Damage(damageInfo);
}

Vec3 SL_DuallisteCloneSkillManager::Resolve_SpawnPosition(const Shared<SL_Dualliste>& owner, const Shared<SL_Base>& target) const
{
    const Shared<TransformCom> ownerTr = owner->Get_Transform();
    const Shared<TransformCom> targetTr = target->Get_Transform();
    if (ownerTr == nullptr || targetTr == nullptr)
        return Vec3::Zero;

    const Vec3 targetPos = targetTr->Get_WorldPosition();
    Vec3 fromTargetToOwner = ownerTr->Get_WorldPosition() - targetPos;
    fromTargetToOwner.y = 0.f;

    if (fromTargetToOwner.LengthSquared() <= 0.0001f)
        fromTargetToOwner = Vec3::Look;
    else
        fromTargetToOwner.Normalize();

    return targetPos + fromTargetToOwner * 1.8f;
}

Vec3 SL_DuallisteCloneSkillManager::Resolve_SpawnRotation(const Vec3& spawnPosition, const Shared<SL_Base>& target) const
{
    const Shared<TransformCom> targetTr = target ? target->Get_Transform() : nullptr;
    if (targetTr == nullptr)
        return Vec3::Zero;

    const Vec3 targetPos = targetTr->Get_WorldPosition();
    Vec3 lookDir = targetPos - spawnPosition;
    lookDir.y = 0.f;

    if (lookDir.LengthSquared() <= 0.0001f)
        return Vec3::Zero;

    lookDir.Normalize();

    const float yaw = XMConvertToDegrees(atan2f(lookDir.x, lookDir.z));
    return Vec3(0.f, yaw, 0.f);
}

Shared<SL_DuallisteCloneSkillManager> SL_DuallisteCloneSkillManager::Create(const Shared<SL_Dualliste>& owner)
{
    Shared<SL_DuallisteCloneSkillManager> instance = make_shared<SL_DuallisteCloneSkillManager>();

    if (FAILED(instance->Initialize(owner)))
        return nullptr;

    return instance;
}

void SL_DuallisteCloneSkillManager::Free()
{
    Reset();
    _owner.reset();

    Base::Free();
}
