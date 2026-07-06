#include "AN_SL_SpawnAttColl.h"

#include <unordered_map>

#include "ClientInstance.h"
#include "ContainerObject.h"
#include "DamageTrigger.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "SL_Dualliste.h"
#include "SL_Mon_AttackState.h"
#include "SL_Mon_StateMachineCom.h"
#include "Spawn_Prefab.h"
#include "TransformCom.h"
#include "Trigger_WorldAction.h"

namespace
{
    size_t Make_RuntimeKey(const Engine::AnimNotifyState* notifyState, GameObject* owner)
    {
        const size_t stateKey = reinterpret_cast<size_t>(notifyState);
        const size_t ownerKey = reinterpret_cast<size_t>(owner);
        return stateKey ^ (ownerKey + 0x9e3779b9u + (stateKey << 6) + (stateKey >> 2));
    }

    unordered_map<size_t, Weak<GameObject>>& Get_SpawnedObjects()
    {
        static std::unordered_map<size_t, Weak<GameObject>> spawnedObjects{};
        return spawnedObjects;
    }

    Vec3 Resolve_LocalOffsetInWorld(TransformCom& transform, const Vec3& localOffset)
    {
        return transform.Get_WorldRight() * localOffset.x
            + transform.Get_WorldUp() * localOffset.y
            + transform.Get_WorldForward() * localOffset.z;
    }
}

IMPLEMENT_REFLECTION(ANS_SL_SpawnAttColl)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "ANS_SL_SpawnAttColl";
    info.category = "SoulLike";

    PROPERTY_STRING("Prefab Name", _prefabName);
    PROPERTY_WSTRING("Layer Tag", _layerTag);
    PROPERTY_VEC3("Local Offset", _localOffset);
    PROPERTY_VEC3("Spawn Rotation", _spawnRotation);
    PROPERTY_VEC3("Spawn Scale", _spawnScale);
    PROPERTY_BOOL("Copy Owner Rotation", _copyOwnerRotation);

    return true;
}

void ANS_SL_SpawnAttColl::On_Begin(const AnimNotifyContext& context)
{
    if (context.isPreview || context.isCinematic || context.owner == nullptr || _prefabName.empty())
        return;

    const Shared<GameObject> spawnOwner = Resolve_SpawnOwner(context.owner);
    if (spawnOwner == nullptr)
        return;

    Remove_SpawnedObject(context.owner);

    auto builder = Spawn_Prefab::Prefab(_prefabName)
        .AtLevel(GAME->Current_LevelIndex())
        .InLayer(_layerTag)
        .Position(Resolve_SpawnWorldPosition(context, spawnOwner))
        .Scale(_spawnScale);

    if (_copyOwnerRotation)
    {
        if (const Shared<TransformCom> ownerTransform = spawnOwner->Get_Transform())
            builder.Rotation(ownerTransform->Get_WorldRotationEuler());
        else
            builder.Rotation(_spawnRotation);
    }
    else
    {
        builder.Rotation(_spawnRotation);
    }

    const Shared<GameObject> spawnedObject = builder.Spawn();
    if (spawnedObject == nullptr)
        return;

    spawnedObject->Set_Owner(spawnOwner);

    if (const Shared<Trigger_WorldAction> worldAction = dynamic_pointer_cast<Trigger_WorldAction>(spawnedObject))
        worldAction->Set_LifeTimeSec(max(context.notifyStateDurationSec, 0.01f));
    if (const Shared<DamageTrigger> damageTrigger = dynamic_pointer_cast<DamageTrigger>(spawnedObject))
    {
        damageTrigger->Set_LifeTimeSec(max(context.notifyStateDurationSec, 0.01f));
        damageTrigger->Refresh_CollisionPresetFromOwner();
    }

    Get_SpawnedObjects()[Make_RuntimeKey(this, context.owner)] = spawnedObject;
}

void ANS_SL_SpawnAttColl::On_Tick(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);
}

void ANS_SL_SpawnAttColl::On_End(const AnimNotifyContext& context)
{
    if (context.owner == nullptr)
        return;

    Remove_SpawnedObject(context.owner);
}

Shared<GameObject> ANS_SL_SpawnAttColl::Resolve_SpawnOwner(GameObject* notifyOwner) const
{
    Shared<GameObject> current = notifyOwner ? notifyOwner->GetSharedPtr<GameObject>() : nullptr;

    while (current)
    {
        if (dynamic_pointer_cast<ContainerObject>(current))
            return current;

        if (current->Get_Owner() == nullptr)
            return current;

        current = current->Get_Owner();
    }

    return nullptr;
}

Vec3 ANS_SL_SpawnAttColl::Resolve_SpawnWorldPosition(const AnimNotifyContext& context, const Shared<GameObject>& spawnOwner) const
{
    const Shared<SL_Dualliste> dualliste = dynamic_pointer_cast<SL_Dualliste>(spawnOwner);
    const Shared<SL_Mon_StateMachineCom> stateMachine = dualliste ? dualliste->Get_Component<SL_Mon_StateMachineCom>() : nullptr;
    const Shared<SL_Mon_AttackState> attackState = stateMachine
        ? dynamic_pointer_cast<SL_Mon_AttackState>(stateMachine->Get_CurrentMonState())
        : nullptr;

    Vec3 cachedPlayerPos{};
    Vec3 cachedPlayerRight{};
    Vec3 cachedPlayerUp{};
    Vec3 cachedPlayerForward{};
    if (attackState && attackState->Try_GetCachedPlayerAnchor(
        cachedPlayerPos,
        cachedPlayerRight,
        cachedPlayerUp,
        cachedPlayerForward))
    {
        return cachedPlayerPos
            + cachedPlayerRight * _localOffset.x
            + cachedPlayerUp * _localOffset.y
            + cachedPlayerForward * _localOffset.z;
    }

    const Shared<TransformCom> ownerTransform = spawnOwner ? spawnOwner->Get_Transform() : nullptr;
    if (ownerTransform == nullptr)
        return Vec3::Zero;

    UNREFERENCED_PARAMETER(context);
    return ownerTransform->Get_WorldPosition() + Resolve_LocalOffsetInWorld(*ownerTransform, _localOffset);
}

void ANS_SL_SpawnAttColl::Remove_SpawnedObject(GameObject* notifyOwner) const
{
    auto& spawnedObjects = Get_SpawnedObjects();
    const size_t key = Make_RuntimeKey(this, notifyOwner);
    const auto iter = spawnedObjects.find(key);
    if (iter == spawnedObjects.end())
        return;

    const Shared<GameObject> spawnedObject = iter->second.lock();
    spawnedObjects.erase(iter);

    if (spawnedObject == nullptr)
        return;

    GAME->Remove_GameObject(GAME->Current_LevelIndex(), spawnedObject);
}
