#include "SL_DuallisteCloneSkillManager.h"

#include "Body_Player.h"
#include "ClientInstance.h"
#include "ModelCom.h"
#include "Player_BattleBase.h"
#include "SL_Base.h"
#include "SL_DamageSystem.h"
#include "SL_Dualliste.h"
#include "SL_Mon_AttackState.h"
#include "SL_Mon_StateMachineCom.h"
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
    const Vec3& goalOffset,
    float fallbackDamageMultiplier,
    float animationSpeed,
    float moveSpeed)
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
    runtime.goalOffset = goalOffset;
    runtime.fallbackDamageMultiplier = fallbackDamageMultiplier;
    runtime.animationSpeed = animationSpeed;
    runtime.moveSpeed = moveSpeed;

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
    Try_ResolveGoalWorldPosition(runtime);

    Shared<Player_BattleBase> actor = Spawn_CloneActor(runtime);
    CHECK_NULL(actor, false);

    runtime.actor = actor;

    const bool shouldMoveBeforeAttack =
        runtime.hasGoalWorldPos &&
        runtime.moveSpeed > 0.f &&
        ((actor->Get_Transform() ? actor->Get_Transform()->Get_WorldPosition() : Vec3::Zero) - runtime.goalWorldPos).LengthSquared() > 0.01f;

    if (!shouldMoveBeforeAttack)
    {
        if (!Play_CloneAnimation(actor, runtime.animationName, runtime.animationSpeed))
        {
            GAME->Remove_GameObject(runtime.levelIndex, actor);
            return false;
        }

        runtime.attackStarted = true;
    }

    _activeClone = std::move(runtime);
    return true;
}

void SL_DuallisteCloneSkillManager::Update_ActiveClone(float timeDelta)
{
    if (!_activeClone.has_value())
        return;

    CloneSkillRuntime& runtime = *_activeClone;
    Shared<Player_BattleBase> actor = runtime.actor.lock();
    if (actor == nullptr)
    {
        _activeClone.reset();
        return;
    }

    if (!runtime.attackStarted)
    {
        if (Update_CloneMovement(runtime, timeDelta))
            return;

        if (!Play_CloneAnimation(actor, runtime.animationName, runtime.animationSpeed))
        {
            Finish_ActiveClone();
            _activeClone.reset();
            return;
        }

        runtime.attackStarted = true;
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

bool SL_DuallisteCloneSkillManager::Try_ResolveGoalWorldPosition(CloneSkillRuntime& runtime) const
{
    Shared<SL_Dualliste> owner = _owner.lock();
    if (owner == nullptr)
        return false;

    const Shared<SL_Mon_StateMachineCom> stateMachine = owner->Get_Component<SL_Mon_StateMachineCom>();
    const Shared<SL_Mon_AttackState> attackState = stateMachine
        ? dynamic_pointer_cast<SL_Mon_AttackState>(stateMachine->Get_CurrentMonState())
        : nullptr;
    if (attackState == nullptr)
        return false;

    Vec3 playerPos = Vec3::Zero;
    Vec3 playerRight = Vec3::Right;
    Vec3 playerUp = Vec3::Up;
    Vec3 playerForward = Vec3::Look;
    if (!attackState->Try_GetCachedPlayerAnchor(playerPos, playerRight, playerUp, playerForward))
        return false;

    runtime.goalWorldPos =
        playerPos +
        playerRight * runtime.goalOffset.x +
        playerUp * runtime.goalOffset.y +
        playerForward * runtime.goalOffset.z;
    runtime.hasGoalWorldPos = true;
    return true;
}

bool SL_DuallisteCloneSkillManager::Update_CloneMovement(CloneSkillRuntime& runtime, float timeDelta) const
{
    if (!runtime.hasGoalWorldPos || runtime.moveSpeed <= 0.f)
        return false;

    Shared<Player_BattleBase> actor = runtime.actor.lock();
    const Shared<TransformCom> transform = actor ? actor->Get_Transform() : nullptr;
    if (transform == nullptr)
        return false;

    const Vec3 currentPos = transform->Get_WorldPosition();
    Vec3 toGoal = runtime.goalWorldPos - currentPos;
    toGoal.y = 0.f;

    const float distanceSq = toGoal.LengthSquared();
    if (distanceSq <= 0.01f)
    {
        transform->Set_WorldPosition(runtime.goalWorldPos);
        return false;
    }

    const float distance = sqrtf(distanceSq);
    const float moveAmount = runtime.moveSpeed * max(timeDelta, 0.f);
    Vec3 moveDir = toGoal / distance;

    Vec3 nextPos = runtime.goalWorldPos;
    if (moveAmount < distance)
        nextPos = currentPos + moveDir * moveAmount;

    nextPos.y = currentPos.y;
    transform->Set_WorldPosition(nextPos);

    Vec3 lookDir = runtime.goalWorldPos - nextPos;
    lookDir.y = 0.f;
    if (lookDir.LengthSquared() > 0.0001f)
    {
        lookDir.Normalize();
        const float yaw = XMConvertToDegrees(atan2f(lookDir.x, lookDir.z));
        transform->Set_WorldRotationEuler(Vec3(0.f, yaw, 0.f));
    }

    return moveAmount < distance;
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
