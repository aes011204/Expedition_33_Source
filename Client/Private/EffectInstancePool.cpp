#include "EffectInstancePool.h"

#include "EffectAssetRuntimeLoader.h"
#include "EffectInstance.h"
#include "GameInstance.h"
#include "Helper_String.h"

NS_BEGIN(Client)

HRESULT EffectInstancePool::Acquire(const AcquireDesc& desc, AttachedHandle& outHandle)
{
    LOG_INFO(
        "[EffectPool] acquire requested. key={}, layer={}, entries={}",
        String::ToString(desc.effectName),
        desc.layerLevelIndex,
        _entries.size());

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

    uint32 enabledEmitterCount = 0;
    if (definition)
    {
        for (const EffectEmitterDefinition& emitter : definition->emitters)
        {
            if (emitter.enabled)
                ++enabledEmitterCount;
        }
    }

    LOG_DEBUG(
        "[EffectPool] acquire completed. key={}, manualRelease={}, enabledEmitters={}",
        String::ToString(desc.effectName),
        outHandle.requiresManualRelease,
        enabledEmitterCount);

    return S_OK;
}

void EffectInstancePool::Release(const AttachedHandle& handle)
{
    if (handle.effectObject == nullptr)
        return;

    PooledInstance* entry = Find_EntryByObject(handle.effectObject);
    if (nullptr == entry || nullptr == entry->instance)
    {
        LOG_WARN("[EffectPool] release skipped. entry not found. key={}", String::ToString(handle.effectName));
        return;
    }

    entry->instance->Stop();
    entry->inUse = false;

    uint32 activeCount = 0;
    uint32 idleCount = 0;
    for (const PooledInstance& pooledEntry : _entries)
    {
        if (pooledEntry.inUse)
            ++activeCount;
        else
            ++idleCount;
    }

    LOG_INFO(
        "[EffectPool] released. key={}, active={}, idle={}",
        String::ToString(entry->effectName),
        activeCount,
        idleCount);
}

void EffectInstancePool::Release_Finished()
{
    uint32 releasedCount = 0;

    for (PooledInstance& entry : _entries)
    {
        if (!entry.inUse || nullptr == entry.instance)
            continue;

        if (!entry.instance->Is_Finished())
            continue;

        entry.instance->Stop();
        entry.inUse = false;
        ++releasedCount;
    }

    if (releasedCount > 0)
        LOG_INFO("[EffectPool] released finished entries. count={}, entries={}", releasedCount, _entries.size());
}

void EffectInstancePool::Clear()
{
    const size_t entryCount = _entries.size();

    for (PooledInstance& entry : _entries)
    {
        if (entry.instance)
            entry.instance->Stop();
    }

    _entries.clear();

    if (entryCount > 0)
        LOG_INFO("[EffectPool] cleared. count={}", entryCount);
}

void EffectInstancePool::Discard_Inactive(const wstring& effectName)
{
    uint32 discardedCount = 0;

    erase_if(
        _entries,
        [&](const PooledInstance& entry)
        {
            if (entry.inUse)
                return false;
            if (entry.effectName != effectName)
                return false;

            if (entry.instance)
            {
                entry.instance->Stop();
                GAME->Remove_GameObject(entry.layerLevelIndex, static_pointer_cast<GameObject>(entry.instance));
            }

            ++discardedCount;
            return true;
        }
    );

    if (discardedCount > 0)
    {
        LOG_INFO(
            "[EffectPool] discarded inactive entries. key={}, count={}, entries={}",
            String::ToString(effectName),
            discardedCount,
            _entries.size());
    }
    else
    {
        LOG_DEBUG("[EffectPool] discard inactive skipped. key={}, entries={}", String::ToString(effectName), _entries.size());
    }
}

HRESULT EffectInstancePool::Acquire_Instance(
    const AcquireDesc& desc,
    Shared<const EffectDefinition>& outDefinition,
    Shared<EffectInstance>& outInstance)
{
    outDefinition = desc.definition ? desc.definition : EffectAssetRuntimeLoader::Load_DefinitionByName(desc.effectName);
    CHECK_NULL(outDefinition, E_FAIL);

    if (PooledInstance* entry = Find_AvailableEntry(desc, outDefinition))
    {
        entry->inUse = true;
        outInstance = entry->instance;

        uint32 activeCount = 0;
        uint32 idleCount = 0;
        for (const PooledInstance& pooledEntry : _entries)
        {
            if (pooledEntry.inUse)
                ++activeCount;
            else
                ++idleCount;
        }

        LOG_INFO(
            "[EffectPool] acquire hit. key={}, active={}, idle={}",
            String::ToString(desc.effectName),
            activeCount,
            idleCount);

        return S_OK;
    }

    LOG_INFO("[EffectPool] acquire miss. key={}, spawning new instance.", String::ToString(desc.effectName));

    CHECK_FAILED(Spawn_NewInstance(desc, outDefinition, outInstance), E_FAIL);

    PooledInstance newEntry{};
    newEntry.effectName = desc.effectName;
    newEntry.definition = outDefinition;
    newEntry.instance = outInstance;
    newEntry.layerLevelIndex = desc.layerLevelIndex;
    newEntry.inUse = true;
    _entries.push_back(newEntry);

    LOG_INFO(
        "[EffectPool] spawned new instance. key={}, layerTag={}, entries={}",
        String::ToString(desc.effectName),
        String::ToString(desc.layerTag),
        _entries.size());

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
            desc.layerLevelIndex,
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

EffectInstancePool::PooledInstance* EffectInstancePool::Find_AvailableEntry(
    const AcquireDesc& desc,
    const Shared<const EffectDefinition>& definition)
{
    for (PooledInstance& entry : _entries)
    {
        if (entry.inUse)
            continue;
        if (entry.effectName != desc.effectName)
            continue;
        if (desc.definition && entry.definition != definition)
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
    auto instance = make_unique<EffectInstancePool>();

    if (nullptr == instance)
    {
        LOG_CRITICAL("Failed to Create : EffectInstancePool");
        return nullptr;
    }

    LOG_INFO("[EffectPool] created.");

    return instance;
}

void EffectInstancePool::Free()
{
    Clear();

    __super::Free();
}

NS_END
