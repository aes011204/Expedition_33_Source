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
    HRESULT Play();
    void Stop();
    HRESULT Reset();
    HRESULT Reload_Definition(const Shared<const EffectDefinition>& definition);
    bool Is_Playing() const;
    bool Is_Finished() const;

public: //## Behavior::PlaybackOptions
    void Set_Loop(bool loop) { _loop = loop; }
    bool Is_Loop() const { return _loop; }
    void Set_PlaybackPaused(bool paused) { _isPlaybackPaused = paused; }
    bool Is_PlaybackPaused() const { return _isPlaybackPaused; }
    void Set_PlaybackSpeed(float speed);
    float Get_PlaybackSpeed() const { return _playbackSpeed; }
    void Set_MaterialRevealOverride(float alphaMultiplier, float alphaErosion);
    void Set_MaterialTintOverride(const vector<uint32>& emitterIds, const Vec4& targetTint, float strength);

public: //## Behavior::ProviderBinding
    void Bind_TrailSampleProvider(
        const Weak<IEffectTrailSampleProvider>& provider,
        const EffectHistorySourceGroupKey& sourceGroupKey = {});
    void Bind_SourcePointSampleProvider(
        const Weak<IEffectSourcePointSampleProvider>& provider,
        const EffectHistorySourceGroupKey& sourceGroupKey = {});

public: //## Accessors::ChildEmitter
    Shared<EffectEmitter> Find_EmitterById(uint32 emitterId) const;

private: //## Types::Lifecycle
    enum class EffectPlaybackState : uint8
    {
        Stopped,
        Playing,
        Finished,
    };

private: //## Data::Definition
    EffectInstanceDesc _desc{};
    Shared<const EffectDefinition> _definition{};

private: //## Data::Lifecycle
    EffectPlaybackState _playbackState{ EffectPlaybackState::Stopped };
    bool _loop{ false };
    bool _isPlaybackPaused{ false };
    float _playbackSpeed{ 1.f };
    uint32 _effectPlaybackSeed{ 0 };

private: //## Data::Children
    vector<Shared<EffectEmitter>> _emitters{};

private: //## Data::SharedHistory
    uint64 _sharedHistoryFrameSerial{};
    EffectHistorySourceGroupKey _trailSourceGroupKey{};
    EffectHistorySourceGroupKey _sourcePointGroupKey{};
    Shared<IEffectTrailSampleProvider> _sharedTrailSampleProvider{};
    Shared<IEffectSourcePointSampleProvider> _sharedSourcePointSampleProvider{};

private: //## Helper::Children
    HRESULT Ready_Emitters();
    bool Try_ReloadEmitters(const Shared<const EffectDefinition>& definition);
    void Sync_Emitters();
    void Evaluate_Finished();
    void Restart_LoopIfNeeded();
    void Clear_Emitters();
    bool Is_SharedSourceHistoryEnabled() const;
    void Advance_SharedHistoryFrame();
    void Clear_SharedHistoryCache();

public:
    static Shared<EffectInstance> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
