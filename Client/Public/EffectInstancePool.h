#pragma once

#include "Base.h"
#include "Client_Defines.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(Engine)
class GameObject;
class EffectInstance;
NS_END

NS_BEGIN(Client)

// TODO: 추가 병목이 확인되면 emitter별 Reset_ForEffectReplay 비용을 더 줄인다.
// 자동 완료 반환은 fire-and-forget entry에만 허용하고, owner-tracked entry는 consumer의 명시 release를 정본으로 둔다.

// EffectInstance 전체를 재사용 단위로 관리하는 Client-local pool helper다.
class EffectInstancePool final : public Base
{
public:
    /**
     * Acquire()에 전달하는 "이 EffectInstance 1개를 어떻게 준비할지" 입력값이다.
     * 여기서 instance 단위란 EffectEmitter 1개가 아니라, 여러 emitter를 소유한 EffectInstance root 1개를 뜻한다.
     */
    struct AcquireDesc
    {
        wstring effectName{};                              // 저장된 effect definition을 찾는 이름이자, pool에서 같은 effect끼리 재사용하기 위한 key다.
        Shared<const EffectDefinition> definition{};       // effectName lookup을 건너뛰고 이 definition을 직접 재생할 때 쓴다. editor preview 갱신용 입력이다.
        uint32 layerLevelIndex{ ETOI(LevelType::Static) }; // 새 EffectInstance를 spawn하거나 inactive instance를 제거할 때 접근할 level layer index다.
        wstring layerTag{};                                // pool에 재사용 가능한 instance가 없을 때 새 EffectInstance를 등록할 layer tag다.
        Vec3 worldPosition{ Vec3::Zero };                  // acquire된 EffectInstance root GameObject에 적용할 시작 월드 위치다.
        Vec3 scale{ Vec3::One };                           // acquire된 EffectInstance root GameObject에 적용할 시작 스케일이다.
        Quat worldRotation{ Quat::Identity };              // acquire된 EffectInstance root GameObject에 적용할 시작 월드 회전이다.
        [[deprecated("Use worldRotation quaternion instead.")]]
        Vec3 worldRotationEuler{ Vec3::Zero };                              // 이전 Euler 입력 호환용 필드다. pool 준비 경로에서는 소비하지 않는다.
        Weak<IEffectTrailSampleProvider> trailSampleProvider{};             // trail emitter가 base/tip sample을 얻는 provider다. pool이 만들지 않고 attachment owner가 넘긴다.
        Weak<IEffectSourcePointSampleProvider> sourcePointSampleProvider{}; // source history emitter가 단일 source sample을 얻는 provider다.
        bool autoReleaseOnFinished{ true };                                 // true면 Release_Finished()가 완료된 instance를 자동 반환할 수 있다.
    };

    /**
     * acquire 결과로 caller가 보관하는 외부 handle이다.
     *
     * effectObject의 C++ 타입은 GameObject지만, 실제 객체는 EffectInstance다.
     * 즉 "GameObject 1개"를 들고 있는 동시에 "EffectInstance 1개 단위"를 가리킨다.
     */
    struct AttachedHandle
    {
        Shared<GameObject> effectObject{};   // acquire된 EffectInstance 1개를 GameObject로 가리킨다. 위치 갱신과 release 대상이다.
        wstring effectName{};                // 이 handle을 만들 때 사용한 effect key다. 표시용 label이 아니라 재사용 bucket을 구분하는 이름이다.
        bool requiresManualRelease{ false }; // true면 Is_Finished() 자동 반환 대신 owner가 Stop/release를 직접 호출해야 한다. 주로 Trail 이펙트용.
    };

    // pool 내부 장부를 외부 debug UI나 로그에서 간단히 확인하기 위한 집계값이다.
    struct DebugSummary
    {
        uint32 entryCount{};  // pool 내부 슬롯 전체 개수다. 슬롯 1개는 EffectInstance 1개를 보관한다.
        uint32 activeCount{}; // caller에게 빌려준 슬롯 수다.
        uint32 idleCount{};   // Stop된 상태로 다음 Acquire()를 기다리는 슬롯 수다.
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

    // 같은 이팩트를 count개 미리 생성해서 idle 상태로 돌려두기
    HRESULT Prewarm(const AcquireDesc& desc, uint32 count);

private:
    /**
     * pool 내부 슬롯 1개다. 슬롯 1개는 재사용 가능한 EffectInstance 1개와, 그 instance를 다시 빌려줘도 되는지 여부를 기록한다.
     * caller에게는 AttachedHandle만 노출하고, 실제 EffectInstance 소유/재사용 상태는 이 entry가 추적한다.
     */
    struct PooledInstance
    {
        wstring effectName{};                        // 이 instance가 속한 pool bucket key다.
        Shared<const EffectDefinition> definition{}; // 재생 시 재사용하는 정적 effect 정의다.
        Shared<EffectInstance> instance{};           // 실제 재사용되는 runtime root다.
        uint32 layerLevelIndex{};                    // instance가 올라간 layer index다.
        wstring layerTag{};                          // instance가 실제 소속된 layer tag다.
        bool inUse{ false };                         // 현재 consumer가 빌려 간 상태인지 나타낸다.
        bool autoReleaseOnFinished{ true };          // 완료 시 전역 자동 반환을 허용하는 fire-and-forget entry인지 기록한다.
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
    void Populate_Handle(
        const AcquireDesc& desc,
        const Shared<const EffectDefinition>& definition,
        const Shared<EffectInstance>& instance,
        AttachedHandle& outHandle) const;
    // 같은 key의 inactive entry를 찾는다. direct definition acquire는 definition identity까지 맞춘다.
    PooledInstance* Find_AvailableEntry(const AcquireDesc& desc, const Shared<const EffectDefinition>& definition);
    // 반환 대상 object에 대응하는 pool entry를 찾는다.
    PooledInstance* Find_EntryByObject(const Shared<GameObject>& effectObject);

public:
    static Unique<EffectInstancePool> Create();
    void Free() override;
};

NS_END
