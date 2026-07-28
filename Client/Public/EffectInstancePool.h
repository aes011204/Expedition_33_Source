#pragma once

#include "Base.h"
#include "Client_Defines.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(Engine)
class GameObject;
class EffectInstance;
NS_END

NS_BEGIN(Client)


class EffectInstancePool final : public Base
{
public: //## Types::Public

    struct AcquireDesc
    {
        wstring effectName{};
        Shared<const EffectDefinition> definition{};
        uint32 layerLevelIndex{ ETOI(LevelType::Static) };
        wstring layerTag{};
        Vec3 worldPosition{ Vec3::Zero };
        Vec3 scale{ Vec3::One };
        Quat worldRotation{ Quat::Identity };
        float playbackSpeed{ 1.f };
        Weak<IEffectTrailSampleProvider> trailSampleProvider{};
        EffectHistorySourceGroupKey trailSampleGroupKey{};
        Weak<IEffectSourcePointSampleProvider> sourcePointSampleProvider{};
        EffectHistorySourceGroupKey sourcePointGroupKey{};
        Weak<IEffectSourcePointSampleProvider> ribbonSourcePointSampleProvider{};
        bool autoReleaseOnFinished{ true };
    };

    struct AttachedHandle
    {
        Shared<GameObject> effectObject{};
        wstring effectName{};
        bool requiresManualRelease{ false };
    };

    struct DebugSummary
    {
        uint32 entryCount{};
        uint32 activeCount{};
        uint32 idleCount{};
    };

    struct PrewarmResult
    {
        uint32 existingCount{};
        uint32 createdCount{};
        uint32 targetCount{};
    };

public:
    EffectInstancePool() = default;
    ~EffectInstancePool() override = default;

public: //## Behavior::Pool
    HRESULT Acquire(const AcquireDesc& desc, AttachedHandle& outHandle);
    HRESULT Reload_Attached(const AcquireDesc& desc, AttachedHandle& handle);
    void Release(const AttachedHandle& handle);
    void Release_Finished();
    void Clear();

public: //## Debug::EditorSession
    void Discard_Inactive(const wstring& effectName);
    uint32 Discard_AllInactive();
    uint32 Discard_All();
    DebugSummary Get_DebugSummary() const;
    string Build_DebugActiveEffectBreakdown(uint32 maxItems = 8) const;

    HRESULT Prewarm(const AcquireDesc& desc, uint32 count);
    HRESULT Prewarm_ToCount(const AcquireDesc& desc, uint32 targetCount, uint32 createBudget, PrewarmResult* outResult = nullptr);
    uint32 Count_Entries(const AcquireDesc& desc, const Shared<const EffectDefinition>& definition) const;

private: //## Types::Pool

    struct PooledInstance
    {
        wstring effectName{};
        Shared<const EffectDefinition> definition{};
        Shared<EffectInstance> instance{};
        uint32 layerLevelIndex{};
        wstring layerTag{};
        bool inUse{ false };
        bool autoReleaseOnFinished{ true };
    };

private: //## Data::Pool
    vector<PooledInstance> _entries{};

private: //## Helper::Pool
    HRESULT Acquire_Instance(
        const AcquireDesc& desc,
        Shared<const EffectDefinition>& outDefinition,
        Shared<EffectInstance>& outInstance);
    HRESULT Spawn_NewInstance(
        const AcquireDesc& desc,
        const Shared<const EffectDefinition>& definition,
        Shared<EffectInstance>& outInstance);
    HRESULT Prepare_AcquiredInstance(
        const AcquireDesc& desc,
        const Shared<const EffectDefinition>& definition,
        const Shared<EffectInstance>& instance);
    void Populate_Handle(
        const AcquireDesc& desc,
        const Shared<const EffectDefinition>& definition,
        const Shared<EffectInstance>& instance,
        AttachedHandle& outHandle) const;
    PooledInstance* Find_AvailableEntry(const AcquireDesc& desc, const Shared<const EffectDefinition>& definition);
    PooledInstance* Find_EntryByObject(const Shared<GameObject>& effectObject);

public:
    static Unique<EffectInstancePool> Create();
    void Free() override;
};

NS_END
