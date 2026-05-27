#include "EffectInstancePool.h"

#include "EffectAssetRuntimeLoader.h"
#include "EffectInstance.h"
#include "GameInstance.h"

NS_BEGIN(Client)

HRESULT EffectInstancePool::Acquire(const AcquireDesc& desc, AttachedHandle& outHandle)
{
    Shared<const EffectDefinition> definition{};
    Shared<EffectInstance> instance{};
    CHECK_FAILED(Acquire_Instance(desc, definition, instance), E_FAIL);
    CHECK_FAILED(Prepare_AcquiredInstance(desc, instance), E_FAIL);

    outHandle.effectObject = static_pointer_cast<GameObject>(instance);
    outHandle.effectName = desc.effectName;
    outHandle.requiresManualRelease = false;

    if (definition)
    {
        for (const EffectEmitterDefinition& emitter : definition->emitters)
        {
            if (!emitter.enabled)
                continue;

            if (emitter.kind == EffectEmitterKind::ComputeTrail)
            {
                outHandle.requiresManualRelease = true;
                break;
            }
        }
    }

    return S_OK;
}

void EffectInstancePool::Release(const AttachedHandle& handle)
{
    if (handle.effectObject == nullptr)
        return;

    PooledInstance* entry = Find_EntryByObject(handle.effectObject);
    if (nullptr == entry || nullptr == entry->instance)
        return;

    entry->instance->Stop();
    entry->inUse = false;
}

void EffectInstancePool::Release_Finished()
{
    for (PooledInstance& entry : _entries)
    {
        if (!entry.inUse || nullptr == entry.instance)
            continue;

        if (!entry.instance->Is_Finished())
            continue;

        entry.instance->Stop();
        entry.inUse = false;
    }
}

void EffectInstancePool::Clear()
{
    for (PooledInstance& entry : _entries)
    {
        if (entry.instance)
            entry.instance->Stop();
    }

    _entries.clear();
}

HRESULT EffectInstancePool::Acquire_Instance(
    const AcquireDesc& desc,
    Shared<const EffectDefinition>& outDefinition,
    Shared<EffectInstance>& outInstance)
{
    outDefinition = EffectAssetRuntimeLoader::Load_DefinitionByName(desc.effectName);
    CHECK_NULL(outDefinition, E_FAIL);

    if (PooledInstance* entry = Find_AvailableEntry(desc.effectName))
    {
        entry->inUse = true;
        outInstance = entry->instance;
        return S_OK;
    }

    CHECK_FAILED(Spawn_NewInstance(desc, outDefinition, outInstance), E_FAIL);

    PooledInstance newEntry{};
    newEntry.effectName = desc.effectName;
    newEntry.definition = outDefinition;
    newEntry.instance = outInstance;
    newEntry.inUse = true;
    _entries.push_back(newEntry);
    return S_OK;
}

HRESULT EffectInstancePool::Spawn_NewInstance(
    const AcquireDesc& desc,
    const Shared<const EffectDefinition>& definition,
    Shared<EffectInstance>& outInstance)
{
    EffectInstanceDesc instanceDesc{};
    instanceDesc.definition = definition;
    instanceDesc.position = desc.worldPosition;

    Shared<GameObject> spawnedObject{};
    CHECK_FAILED(
        GAME->Add_GameObject(
            ETOI(LevelType::Static),
            L"GameObject_EffectInstance",
            GAME->Current_LevelIndex(),
            desc.layerTag,
            spawnedObject,
            &instanceDesc),
        E_FAIL
    );

    outInstance = dynamic_pointer_cast<EffectInstance>(spawnedObject);
    CHECK_NULL(outInstance, E_FAIL);
    return S_OK;
}

HRESULT EffectInstancePool::Prepare_AcquiredInstance(const AcquireDesc& desc, const Shared<EffectInstance>& instance)
{
    CHECK_NULL(instance, E_FAIL);
    instance->Bind_TrailSampleProvider(desc.trailSampleProvider);
    CHECK_FAILED(instance->Reset(), E_FAIL);

    const Shared<TransformCom> transform = instance->Get_Transform();
    CHECK_NULL(transform, E_FAIL);
    transform->Set_WorldPosition(desc.worldPosition);

    CHECK_FAILED(instance->Play(), E_FAIL);
    return S_OK;
}

EffectInstancePool::PooledInstance* EffectInstancePool::Find_AvailableEntry(const wstring& effectName)
{
    for (PooledInstance& entry : _entries)
    {
        if (entry.inUse)
            continue;
        if (entry.effectName != effectName)
            continue;
        if (nullptr == entry.instance)
            continue;

        return &entry;
    }

    return nullptr;
}

EffectInstancePool::PooledInstance* EffectInstancePool::Find_EntryByObject(const Shared<GameObject>& effectObject)
{
    for (PooledInstance& entry : _entries)
    {
        if (entry.instance == effectObject)
            return &entry;
    }

    return nullptr;
}

Unique<EffectInstancePool> EffectInstancePool::Create()
{
    return make_unique<EffectInstancePool>();
}

void EffectInstancePool::Free()
{
    Clear();

    __super::Free();
}

NS_END
