#pragma once
#include "EffectEmitter.h"
#include "EffectEmitterFactory.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(Engine)

class ENGINE_DLL EffectInstance final : public GameObject
{
    GENERATED_GAMEOBJECT(EffectInstance, ObjectType::Effect)

public:
    EffectInstance(const ComPtr<Device>& device, const ComPtr<Context>& context);
    EffectInstance(const EffectInstance& prototype);
    ~EffectInstance() override;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void BeginPlay() override;
    void Priority_Update(float timeDelta) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;

public: //## Behavior::Lifecycle
    HRESULT Play();           // effect root를 활성 재생 상태로 전환한다.
    void Stop();              // effect root를 명시 정지 상태로 전환한다.
    HRESULT Reset();          // 같은 definition으로 다시 재생할 수 있도록 root와 child runtime 상태를 초기화한다.
    bool Is_Playing() const;  // 현재 update/render fan-out 대상인지 반환한다.
    bool Is_Finished() const; // one-shot 완료 후 pool 반환 후보인지 반환한다.
    void Bind_TrailSampleProvider(const Weak<IEffectTrailSampleProvider>& provider); // 현재 acquire context가 공급한 trail sample provider를 child emitter에 다시 전파한다.

private: //## Types::Lifecycle
    enum class EffectPlaybackState : uint8
    {
        Stopped,  // 재생 전이거나 명시 정지된 상태다.
        Playing,  // child emitter update/render fan-out을 수행하는 상태다.
        Finished, // 모든 자동 완료형 child emitter가 완료된 상태다.
    };

private: //## Data::Definition
    EffectInstanceDesc _desc{};                   // root clone 시 전달받는 runtime desc다.
    Shared<const EffectDefinition> _definition{}; // root가 재생할 정적 effect 정의다.

private: //## Data::Lifecycle
    EffectPlaybackState _playbackState{ EffectPlaybackState::Stopped }; // pool 재사용 판단에 쓰는 root 재생 상태다.

private: //## Data::Children
    vector<Shared<EffectEmitter>> _emitters{}; // root가 직접 소유하는 child emitter 목록이다.

private: //## Helpers
    HRESULT Ready_Emitters(); // definition emitters를 concrete runtime child로 생성한다.
    void Sync_Emitters();     // root visibility/transform을 child emitter에 fan-out한다.
    void Evaluate_Finished(); // child emitter 완료 상태를 종합해 root 완료 상태를 갱신한다.
    void Clear_Emitters();    // root가 소유한 child 목록을 정리한다.

public:
    static Shared<EffectInstance> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
