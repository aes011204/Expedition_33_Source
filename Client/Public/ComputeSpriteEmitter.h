#pragma once
#include "Client_Defines.h"
#include "EffectEmitter.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(Engine)
class ShaderCom;
class Texture;
class VIBufferCom_ComputePointParticle;
NS_END

NS_BEGIN(Client)

class ComputeSpriteEmitter final : public EffectEmitter
{
    GENERATED_GAMEOBJECT(ComputeSpriteEmitter, ObjectType::EffectEmitter);

public:
    ComputeSpriteEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ComputeSpriteEmitter(const ComputeSpriteEmitter& prototype);
    ~ComputeSpriteEmitter() override;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;

private: //## Types::PlaybackRuntime
    enum class PlaybackState : uint8
    {
        Delayed,
        Playing,
        Completed,
    };

    struct PlaybackTick
    {
        float computeDeltaTime{};     // 이번 frame에서 particle simulation에 흘려보낼 시간이다.
        uint32 spawnRequest{};        // 이번 frame에 활성화할 particle 수다.
        bool killActiveParticles{};   // 이번 frame에 active particle을 모두 죽일지 나타낸다.
    };

private: //## Static Values
    static constexpr const char* kFallbackTextureId{ "Texture_Effect_DefaultTexture" };
    static constexpr const char* kEffectSpriteShaderId{ "Shader_EffectSprite" };
    static constexpr int kOpacitySourceAlpha{ 0 };
    static constexpr int kOpacitySourceRed{ 1 };
    static constexpr int kOpacitySourceLuminance{ 2 };

private: //## Data::Components
    Shared<ShaderCom> _shader{};                          // EffectSprite material effect shader.
    Shared<Texture> _texture{};                           // compute sprite effect main texture.
    Shared<Texture> _noiseTexture{};                      // 선택 noise texture. 없으면 material noise를 비활성화한다.
    Shared<VIBufferCom_ComputePointParticle> _viBuffer{}; // compute output을 instance draw로 제출하는 검증 버퍼.

private: //## Data::Desc
    ComputeSpriteEmitterDesc _desc{};               // clone 생성 시 전달받은 compute sprite emitter 규칙이다.
    int _opacitySourceIndex{ kOpacitySourceAlpha }; // shader opacity source selector.

private: //## Data::SpawnRuntime
    vector<bool> _burstExecuted{}; // burst 목록의 1회 실행 여부를 emitter runtime 동안 유지한다.
    float _emitterElapsedTime{};   // spawn rate/burst 평가에 사용할 emitter-local 누적 시간이다.
    float _spawnAccumulator{};     // fractional spawn rate를 프레임 사이에 보존한다.

private: //## Data::PlaybackRuntime
    PlaybackState _playbackState{ PlaybackState::Delayed }; // delay/playing/completed 중 현재 emitter 재생 상태다.
    float _loopElapsedTime{};                               // 현재 loop 안에서 delay를 포함해 흐른 시간이다.
    uint32 _loopIndex{};                                    // 0부터 시작하는 현재 loop index다.
    bool _completedKillDispatched{};                        // 완료 kill 요청을 한 번만 보내기 위한 latch다.

private: //## Helper::Setup
    HRESULT Ready_Components();                                              // effect에 필요한 shader/texture/compute buffer component를 준비한다.
    HRESULT Ready_TextureComponent(string& outResolvedTextureId);            // compute sprite main texture를 준비하고 fallback 결과를 기록한다.
    HRESULT Ready_ShaderComponent();                                         // compute sprite effect가 사용할 EffectSprite shader를 준비한다.
    HRESULT Ready_NoiseTextureComponent(const string& outResolvedTextureId); // 선택 noise texture를 준비하고 없으면 noise path를 비활성화한다.

private: //## Helper::Render
    HRESULT Bind_ShaderResources();         // EffectSprite shader에 camera/texture resource를 바인딩한다.
    HRESULT Bind_MaterialResources();       // EffectSprite material 파라미터를 shader에 바인딩한다.
    int Resolve_OpacitySourceIndex() const; // authoring opacitySource 문자열을 shader selector 값으로 변환한다.

private: //## Helper::Desc
    ComputePointParticleDesc Make_ComputePointParticleDesc() const; // compute sprite effect용 particle 규칙을 만든다.
    void Sync_WorldCenterToComputeBuffer();                         // emitter transform world position을 compute center에 반영한다.

private: //## Helper::SpawnRuntime
    uint32 Consume_SpawnRequest(float timeDelta); // 이번 frame에 새로 활성화할 particle 수를 계산한다.

private: //## Helper::PlaybackRuntime
    void Reset_PlaybackRuntime();              // emitter timing runtime state를 초기화한다.
    PlaybackTick Advance_Playback(float timeDelta); // delay/duration/loop 정책을 진행하고 compute tick을 만든다.
    void Start_NextLoop();                     // loop-local spawn/burst state를 초기화하고 다음 loop로 넘어간다.
    float Resolve_CurrentLoopDelay() const;    // 현재 loop에 적용할 delay를 반환한다.
    float Resolve_Duration() const;            // 0 이하 duration으로 인한 무한 루프를 막는 안전 duration을 반환한다.
    bool Has_NextLoop() const;                 // loopCount 정책상 다음 loop가 있는지 확인한다.
    bool Can_SubmitRender() const;             // delay/completed kill 정책상 render 제출 가능한지 확인한다.

protected:
    void On_EffectTransformSynced() override;                       // root/local-space sync 직후 compute center를 즉시 갱신한다.

public:
    static Shared<ComputeSpriteEmitter> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
