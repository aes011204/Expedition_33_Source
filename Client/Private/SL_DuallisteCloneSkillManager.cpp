#include "SL_DuallisteCloneSkillManager.h"

#include "Body_Player.h"
#include "ClientInstance.h"
#include "ContainerObject.h"
#include "EffectCom.h"
#include "ModelCom.h"
#include "Monster_Part.h"
#include "SL_Base.h"
#include "SL_DamageSystem.h"
#include "SL_Dualliste.h"
#include "Spawn_Prefab.h"
#include "TransformCom.h"

namespace
{
    constexpr float kCloneBaseDamage = 100.f;
    constexpr char  kCloneDissolveNoise[] = "T_Perlin_Noise_M";

    Shared<ModelCom> Resolve_CloneModel(const Shared<GameObject>& actor)
    {
        if (actor == nullptr)
            return nullptr;

        const Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(actor);
        if (container == nullptr)
            return nullptr;

        PartObject* bodyPart = container->Get_PartObject(L"Part_Body");
        if (bodyPart == nullptr)
            return nullptr;

        if (Body_Player* bodyPlayer = dynamic_cast<Body_Player*>(bodyPart))
            return bodyPlayer->Get_Model();

        if (Monster_Part* monsterPart = dynamic_cast<Monster_Part*>(bodyPart))
            return monsterPart->Get_Model();

        return nullptr;
    }

    void Start_CloneDissolve(const Shared<GameObject>& actor, float dissolveDuration)
    {
        if (actor == nullptr)
            return;

        if (const Shared<EffectCom> rootEffectCom = actor->Get_Component<EffectCom>())
            rootEffectCom->Start_VanishDissolve(dissolveDuration, kCloneDissolveNoise, true);

        const Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(actor);
        if (container == nullptr)
            return;

        for (const auto& [partTag, partObject] : container->Get_PartObjects())
        {
            if (partObject == nullptr)
                continue;

            if (const Shared<EffectCom> partEffectCom = partObject->Get_Component<EffectCom>())
                partEffectCom->Start_VanishDissolve(dissolveDuration, kCloneDissolveNoise, true);
        }
    }
}

HRESULT SL_DuallisteCloneSkillManager::Initialize(const Shared<SL_Dualliste>& owner)
{
    CHECK_NULL(owner, E_FAIL);

    _owner = owner;
    _activeClones.clear();

    return S_OK;
}

void SL_DuallisteCloneSkillManager::Update(float timeDelta)
{
    Update_ActiveClones(timeDelta);
}

void SL_DuallisteCloneSkillManager::Reset()
{
    for (CloneSkillRuntime& runtime : _activeClones)
    {
        if (Shared<GameObject> actor = runtime.actor.lock())
            GAME->Remove_GameObject(runtime.levelIndex, actor);
    }

    _activeClones.clear();
}

bool SL_DuallisteCloneSkillManager::Request_CloneSkill(
    const string& prefabName,
    const string& skillKey,
    const string& animationName,
    const Shared<SL_Base>& target,
    const Vec3& spawnOffset,
    float goalOffset,
    float fallbackDamageMultiplier,
    float animationSpeed,
    float moveSpeed,
    float dissolveDuration)
{
    if (_activeClones.size() >= kMaxActiveClones)
        return false;

    if (prefabName.empty() || animationName.empty() || target == nullptr || target->Is_Dead())
        return false;

    CloneSkillRuntime runtime{};
    runtime.target = target;
    runtime.levelIndex = GAME->Current_LevelIndex();
    runtime.prefabName = prefabName;
    runtime.skillKey = skillKey;
    runtime.animationName = animationName;
    runtime.spawnOffset = spawnOffset;
    runtime.goalOffset = max(0.f, goalOffset);
    runtime.fallbackDamageMultiplier = fallbackDamageMultiplier;
    runtime.animationSpeed = animationSpeed;
    runtime.moveSpeed = moveSpeed;
    runtime.dissolveDuration = max(0.01f, dissolveDuration);

    return Begin_CloneSkill(std::move(runtime));
}

bool SL_DuallisteCloneSkillManager::Notify_Attack(GameObject* notifyOwner)
{
    CloneSkillRuntime* runtime = Find_CloneByNotifyOwner(notifyOwner);
    if (runtime == nullptr)
        return false;

    if (!runtime->attackApplied)
    {
        Apply_CloneDamage(*runtime);
        runtime->attackApplied = true;
    }

    return true;
}

bool SL_DuallisteCloneSkillManager::Begin_CloneSkill(CloneSkillRuntime runtime)
{
    Try_ResolveSpawnWorldPosition(runtime);
    Try_ResolveGoalWorldPosition(runtime);

    Shared<GameObject> actor = Spawn_CloneActor(runtime);
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

    _activeClones.push_back(std::move(runtime));
    return true;
}

void SL_DuallisteCloneSkillManager::Update_ActiveClones(float timeDelta)
{
    if (_activeClones.empty())
        return;

    for (size_t i = 0; i < _activeClones.size();)
    {
        CloneSkillRuntime& runtime = _activeClones[i];
        Shared<GameObject> actor = runtime.actor.lock();
        if (actor == nullptr)
        {
            _activeClones.erase(_activeClones.begin() + static_cast<ptrdiff_t>(i));
            continue;
        }

        if (runtime.isDissolving)
        {
            runtime.dissolveElapsed += max(timeDelta, 0.f);
            if (runtime.dissolveElapsed >= runtime.dissolveDuration)
            {
                GAME->Remove_GameObject(runtime.levelIndex, actor);
                _activeClones.erase(_activeClones.begin() + static_cast<ptrdiff_t>(i));
                continue;
            }

            ++i;
            continue;
        }

        if (!runtime.attackStarted)
        {
            if (Update_CloneMovement(runtime, timeDelta))
            {
                ++i;
                continue;
            }

            if (!Play_CloneAnimation(actor, runtime.animationName, runtime.animationSpeed))
            {
                GAME->Remove_GameObject(runtime.levelIndex, actor);
                _activeClones.erase(_activeClones.begin() + static_cast<ptrdiff_t>(i));
                continue;
            }

            runtime.attackStarted = true;
        }

        Shared<ModelCom> model = Resolve_CloneModel(actor);
        if (model != nullptr && !model->Is_AnimationFinished())
        {
            ++i;
            continue;
        }

        Finish_Clone(runtime);
        ++i;
    }
}

void SL_DuallisteCloneSkillManager::Finish_Clone(CloneSkillRuntime& runtime)
{
    if (runtime.isDissolving)
        return;

    Shared<GameObject> actor = runtime.actor.lock();
    if (actor == nullptr)
        return;

    Start_CloneDissolve(actor, runtime.dissolveDuration);
    runtime.isDissolving = true;
    runtime.dissolveElapsed = 0.f;
}

Shared<GameObject> SL_DuallisteCloneSkillManager::Spawn_CloneActor(const CloneSkillRuntime& runtime)
{
    Shared<SL_Dualliste> owner = _owner.lock();
    Shared<SL_Base> target = runtime.target.lock();
    if (owner == nullptr || target == nullptr)
        return nullptr;

    const Vec3 spawnPosition = runtime.hasSpawnWorldPos
        ? runtime.spawnWorldPos
        : Resolve_SpawnPosition(owner, runtime.spawnOffset);
    const Vec3 spawnRotation = Resolve_SpawnRotation(spawnPosition, target);

    Shared<GameObject> object = Spawn_Prefab::Prefab(runtime.prefabName)
        .AtLevel(runtime.levelIndex)
        .InLayer(TEXT("Layer_Player"))
        .Position(spawnPosition)
        .Rotation(spawnRotation)
        .Spawn();

    if (const Shared<SL_Dualliste> cloneDualliste = dynamic_pointer_cast<SL_Dualliste>(object))
        cloneDualliste->Set_CloneVisualOnly(true);

    return object;
}

bool SL_DuallisteCloneSkillManager::Play_CloneAnimation(
    const Shared<GameObject>& actor,
    const string& animationName,
    float animationSpeed)
{
    if (actor == nullptr)
        return false;

    Shared<ModelCom> model = Resolve_CloneModel(actor);
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
    Shared<SL_Base> target = runtime.target.lock();
    const Shared<TransformCom> targetTr = target ? target->Get_Transform() : nullptr;
    if (targetTr == nullptr)
        return false;

    if (!runtime.hasSpawnWorldPos)
        Try_ResolveSpawnWorldPosition(runtime);

    const Vec3 targetPos = targetTr->Get_WorldPosition();
    const Vec3 startPos = runtime.hasSpawnWorldPos ? runtime.spawnWorldPos : targetPos;

    Vec3 toTarget = targetPos - startPos;

    if (toTarget.LengthSquared() <= 0.0001f)
        toTarget = Vec3::Look;
    else
        toTarget.Normalize();

    runtime.goalWorldPos = targetPos - toTarget * runtime.goalOffset;
    runtime.goalWorldPos.y = targetPos.y;
    runtime.hasGoalWorldPos = true;
    return true;
}

bool SL_DuallisteCloneSkillManager::Try_ResolveSpawnWorldPosition(CloneSkillRuntime& runtime) const
{
    Shared<SL_Dualliste> owner = _owner.lock();
    if (owner == nullptr)
        return false;

    runtime.spawnWorldPos = Resolve_SpawnPosition(owner, runtime.spawnOffset);
    runtime.hasSpawnWorldPos = true;
    return true;
}

bool SL_DuallisteCloneSkillManager::Update_CloneMovement(CloneSkillRuntime& runtime, float timeDelta) const
{
    if (!runtime.hasGoalWorldPos || runtime.moveSpeed <= 0.f)
        return false;

    Shared<GameObject> actor = runtime.actor.lock();
    const Shared<TransformCom> transform = actor ? actor->Get_Transform() : nullptr;
    if (transform == nullptr)
        return false;

    const Vec3 currentPos = transform->Get_WorldPosition();
    Vec3 toGoal = runtime.goalWorldPos - currentPos;

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

    transform->Set_WorldPosition(nextPos);

    Vec3 lookDir = runtime.goalWorldPos - nextPos;
    Vec3 flatLookDir = lookDir;
    flatLookDir.y = 0.f;
    if (flatLookDir.LengthSquared() > 0.0001f)
    {
        flatLookDir.Normalize();
        const float yaw = XMConvertToDegrees(atan2f(flatLookDir.x, flatLookDir.z));
        transform->Set_WorldRotationEuler(Vec3(0.f, yaw, 0.f));
    }

    return moveAmount < distance;
}

SL_DuallisteCloneSkillManager::CloneSkillRuntime* SL_DuallisteCloneSkillManager::Find_CloneByNotifyOwner(GameObject* notifyOwner)
{
    if (notifyOwner == nullptr)
        return nullptr;

    for (CloneSkillRuntime& runtime : _activeClones)
    {
        Shared<GameObject> actor = runtime.actor.lock();
        if (actor == nullptr)
            continue;

        Shared<GameObject> current = notifyOwner->GetSharedPtr<GameObject>();
        while (current != nullptr)
        {
            if (current.get() == actor.get())
                return &runtime;

            current = current->Get_Owner();
        }
    }

    return nullptr;
}

void SL_DuallisteCloneSkillManager::Apply_CloneDamage(const CloneSkillRuntime& runtime)
{
    Shared<GameObject> actor = runtime.actor.lock();
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

Vec3 SL_DuallisteCloneSkillManager::Resolve_SpawnPosition(const Shared<SL_Dualliste>& owner, const Vec3& spawnOffset) const
{
    const Shared<TransformCom> ownerTr = owner ? owner->Get_Transform() : nullptr;
    if (ownerTr == nullptr)
        return Vec3::Zero;

    return ownerTr->Get_WorldPosition() +
        ownerTr->Get_WorldRight() * spawnOffset.x +
        ownerTr->Get_WorldUp() * spawnOffset.y +
        ownerTr->Get_WorldForward() * spawnOffset.z;
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
