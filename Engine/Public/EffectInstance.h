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
    // 기존 root object를 유지한 채 새 definition으로 child emitter 구성을 다시 만든다.
    HRESULT Reload_Definition(const Shared<const EffectDefinition>& definition);
    bool Is_Playing() const;  // 현재 update/render fan-out 대상인지 반환한다.
    bool Is_Finished() const; // one-shot 완료 후 pool 반환 후보인지 반환한다.
    void Set_Loop(bool loop) { _loop = loop; }
    bool Is_Loop() const { return _loop; }
    void Set_PlaybackPaused(bool paused) { _isPlaybackPaused = paused; } // 튜토리얼 freeze는 Stop이 아니라 update만 멈춘다.
    bool Is_PlaybackPaused() const { return _isPlaybackPaused; }
    void Bind_TrailSampleProvider(const Weak<IEffectTrailSampleProvider>& provider); // 현재 acquire context가 공급한 trail sample provider를 child emitter에 다시 전파한다.
    Shared<EffectEmitter> Find_EmitterById(uint32 emitterId) const; // 같은 effect root 안의 child emitter를 follower source id로 찾는다.

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
    bool _loop{ false }; // 에디터 배치 이펙트가 끝난 뒤 같은 definition으로 다시 재생할지 정한다.
    bool _isPlaybackPaused{ false }; // 현재 프레임을 화면에 남긴 채 emitter 시간 진행만 막는다.
    uint32 _effectPlaybackSeed{ 0 }; // effect root 재생 1회 단위 random seed다.

private: //## Data::Children
    vector<Shared<EffectEmitter>> _emitters{}; // root가 직접 소유하는 child emitter 목록이다.

private: //## Helpers
    HRESULT Ready_Emitters(); // definition emitters를 concrete runtime child로 생성한다.
    bool Try_ReloadEmitters(const Shared<const EffectDefinition>& definition); // 기존 child가 새 definition을 받을 수 있으면 재생성 없이 갱신한다.
    void Sync_Emitters();     // root visibility/transform을 child emitter에 fan-out한다.
    void Evaluate_Finished(); // child emitter 완료 상태를 종합해 root 완료 상태를 갱신한다.
    void Restart_LoopIfNeeded(); // Loop가 켜진 배치 이펙트를 완료 직후 다시 재생한다.
    void Clear_Emitters();    // root가 소유한 child 목록을 정리한다.

public:
    static Shared<EffectInstance> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
