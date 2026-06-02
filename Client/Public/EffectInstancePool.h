#pragma once

#include "Base.h"
#include "Client_Defines.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(Engine)
class GameObject;
class EffectInstance;
NS_END

NS_BEGIN(Client)

// TODO: 현재 pool은 EffectInstance 단위 reuse까지만 닫혀 있다. emitter-local finer-grained reuse와 release policy 고도화는 후속 phase에서 분리해 다룬다.

// EffectInstance 전체를 재사용 단위로 관리하는 Client-local pool helper다.
class EffectInstancePool final : public Base
{
public:
    struct AcquireDesc
    {
        wstring effectName{};                                   // 어떤 effect 정의를 재생할지 식별하는 이름이다.
        Shared<const EffectDefinition> definition{};            // editor preview처럼 저장된 asset이 아닌 definition을 직접 재생할 때 사용한다.
        uint32 layerLevelIndex{ ETOI(LevelType::Static) };      // 신규 spawn과 stale entry 제거에 사용할 layer index다.
        wstring layerTag{};                                     // acquire 실패 시 신규 spawn을 넣을 레이어다.
        Vec3 worldPosition{ Vec3::Zero };                       // acquire 직후 root에 반영할 월드 위치다.
        Vec3 scale{ Vec3::One };                                // acquire 직후 root에 반영할 스케일이다.
        Vec3 worldRotation = Vec3::Zero;
        Weak<IEffectTrailSampleProvider> trailSampleProvider{}; // attachment/gameplay owner가 공급하는 trail sample provider weak ref다.
    };

    struct AttachedHandle
    {
        Shared<GameObject> effectObject{};   // 실제로 follow/release 대상이 되는 acquired effect root다.
        wstring effectName{};                // release 시 원래 pool bucket을 찾기 위한 key다.
        bool requiresManualRelease{ false }; // true면 자동 완료 대신 명시 Stop/release를 기다린다.
    };

    struct DebugSummary
    {
        uint32 entryCount{};
        uint32 activeCount{};
        uint32 idleCount{};
    };

public:
    EffectInstancePool() = default;
    ~EffectInstancePool() override = default;

public:
    // 기존 instance 재사용 또는 신규 spawn 후 ready 상태로 돌려준다.
    HRESULT Acquire(const AcquireDesc& desc, AttachedHandle& outHandle);
    // 이미 acquired 된 instance root를 유지한 채 새 definition으로 preview/runtime 내용을 갱신한다.
    HRESULT Reload_Attached(const AcquireDesc& desc, AttachedHandle& handle);
    // acquired instance를 Stop 후 inactive 상태로 반환한다.
    void Release(const AttachedHandle& handle);
    // 자동 완료 가능한 instance들을 순회하며 pool로 되돌린다.
    void Release_Finished();
    // pool bookkeeping과 재사용 엔트리를 비운다.
    void Clear();

public:
    // [Effect Editor용] 같은 key의 inactive entry만 level에서 제거해 mutable preview definition 재사용을 막는다.
    void Discard_Inactive(const wstring& effectName);
    DebugSummary Get_DebugSummary() const;

private:
    struct PooledInstance
    {
        wstring effectName{};                        // 이 instance가 속한 pool bucket key다.
        Shared<const EffectDefinition> definition{}; // 재생 시 재사용하는 정적 effect 정의다.
        Shared<EffectInstance> instance{};           // 실제 재사용되는 runtime root다.
        uint32 layerLevelIndex{};                    // instance가 올라간 layer index다.
        bool inUse{ false };                         // 현재 consumer가 빌려 간 상태인지 나타낸다.
    };

private:
    vector<PooledInstance> _entries{}; // 현재 pool이 관리하는 EffectInstance 재사용 엔트리 목록이다.

private:
    // key 기준 instance 확보를 담당한다.
    HRESULT Acquire_Instance(
        const AcquireDesc& desc,
        Shared<const EffectDefinition>& outDefinition,
        Shared<EffectInstance>& outInstance);
    // pool miss 시 기존 spawn 경로로 새 instance를 만든다.
    HRESULT Spawn_NewInstance(
        const AcquireDesc& desc,
        const Shared<const EffectDefinition>& definition,
        Shared<EffectInstance>& outInstance);
    // 다음 재생 전에 Reset/position/Play를 적용한다.
    HRESULT Prepare_AcquiredInstance(
        const AcquireDesc& desc,
        const Shared<const EffectDefinition>& definition,
        const Shared<EffectInstance>& instance);
    // definition 내용에 따라 handle 반환 정책을 갱신한다.
    void Populate_Handle(const AcquireDesc& desc, const Shared<const EffectDefinition>& definition, const Shared<EffectInstance>& instance, AttachedHandle& outHandle) const;
    // 같은 key의 inactive entry를 찾는다. direct definition acquire는 definition identity까지 맞춘다.
    PooledInstance* Find_AvailableEntry(const AcquireDesc& desc, const Shared<const EffectDefinition>& definition);
    // 반환 대상 object에 대응하는 pool entry를 찾는다.
    PooledInstance* Find_EntryByObject(const Shared<GameObject>& effectObject);

public:
    static Unique<EffectInstancePool> Create();
    void Free() override;
};

NS_END
