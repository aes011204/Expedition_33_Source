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

public:
    HRESULT Reset_ForEffectReplay() override; // effect root 재사용 시 sprite playback runtime을 초기화한다.
    bool Is_EffectFinished() const override;  // sprite playback이 마지막 loop를 끝냈는지 반환한다.

public: //## Debug::EditorSession
    static void Set_InfluenceOutlineDebugEnabled(bool enabled); // EffectEditor 세션에서 sprite influence outline shader overlay를 켜고 끈다.
    static bool Is_InfluenceOutlineDebugEnabled();

protected:
    void On_EffectTransformSynced() override; // root/local-space sync 직후 compute center를 즉시 갱신한다.

private: //## Types::PlaybackRuntime
    enum class PlaybackState : uint8
    {
        Delayed,
        Playing,
        Completed,
    };

    struct PlaybackTick
    {
        float computeDeltaTime{};   // 이번 frame에서 particle simulation에 흘려보낼 시간이다.
        uint32 spawnRequest{};      // 이번 frame에 활성화할 particle 수다.
        bool killActiveParticles{}; // 이번 frame에 active particle을 모두 죽일지 나타낸다.
    };

private: //## Static Values
    static constexpr auto kFallbackTexturePath{ "Effects/Textures/Shared/DefaultTexture.dds" };
    static constexpr auto kEffectSpriteShaderId{ "Shader_EffectSprite" };
    static constexpr auto kEffectDistortionSpriteShaderId{ "Shader_EffectDistortionSprite" }; // SpriteDistortion family가 사용할 전용 shader id다.
    static constexpr int kOpacitySourceAlpha{ 0 };
    static constexpr int kOpacitySourceRed{ 1 };
    static constexpr int kOpacitySourceLuminance{ 2 };
    static constexpr float kInfluenceOutlinePixelWidthScale{ 2.f };
    inline static bool _isInfluenceOutlineDebugEnabled{ false };

private: //## Data::Components
    Shared<ShaderCom> _shader{};                          // EffectSprite material effect shader.
    Shared<Texture> _texture{};                           // compute sprite effect main texture.
    Shared<Texture> _noiseTexture{};                      // 선택 noise texture. 없으면 material noise를 비활성화한다.
    Shared<Texture> _maskTexture{};                       // 선택 mask texture. 없으면 material mask/erosion을 비활성화한다.
    Shared<Texture> _flowTexture{};                       // 선택 flow texture. 없으면 distortion offset을 0으로 처리한다.
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
    HRESULT Ready_Components();                                             // effect에 필요한 shader/texture/compute buffer component를 준비한다.
    HRESULT Ready_TextureComponent(string& outResolvedTexturePath);         // compute sprite main texture를 준비하고 fallback 결과를 기록한다.
    HRESULT Ready_ShaderComponent();                                        // compute sprite effect가 사용할 EffectSprite shader를 준비한다.
    HRESULT Ready_NoiseTextureComponent(const string& resolvedTexturePath); // 선택 noise texture를 준비하고 없으면 noise path를 비활성화한다.
    HRESULT Ready_MaskTextureComponent(const string& resolvedTexturePath);  // 선택 mask texture를 준비하고 없으면 alpha mask를 비활성화한다.
    HRESULT Ready_FlowTextureComponent(const string& resolvedTexturePath);  // SpriteDistortion용 flow texture를 준비하고 없으면 neutral flow로 둔다.

private: //## Helper::Render
    HRESULT Bind_ShaderResources();          // EffectSprite shader에 camera/texture resource를 바인딩한다.
    HRESULT Bind_MaterialResources();        // EffectSprite material 파라미터를 shader에 바인딩한다.
    HRESULT Bind_DistortionResources();      // SpriteDistortion shader에 scene snapshot과 flow 파라미터를 바인딩한다.
    bool Is_DistortionFamily() const;        // 현재 material family가 sprite distortion 경로인지 확인한다.
    RenderGroup Resolve_RenderGroup() const; // material family에 맞는 renderer group을 반환한다.
    const char* Resolve_ShaderId() const;    // material family에 맞는 shader prototype id를 반환한다.
    uint32 Resolve_ShaderPassIndex() const;  // 현재 shader에서 사용할 pass index를 반환한다.
    int Resolve_OpacitySourceIndex() const;  // authoring opacitySource 문자열을 shader selector 값으로 변환한다.
    uint32 Resolve_BlendPassIndex() const;   // material blend mode에 맞는 EffectSprite shader pass를 고른다.

private: //## Helper::Desc
    ComputePointParticleDesc Make_ComputePointParticleDesc() const; // compute sprite effect용 particle 규칙을 만든다.
    void Sync_WorldCenterToComputeBuffer();                         // emitter transform world position을 compute center에 반영한다.

private: //## Helper::SpawnRuntime
    uint32 Consume_SpawnRequest(float timeDelta); // 이번 frame에 새로 활성화할 particle 수를 계산한다.

private: //## Helper::PlaybackRuntime
    void Reset_PlaybackRuntime();                   // emitter timing runtime state를 초기화한다.
    PlaybackTick Advance_Playback(float timeDelta); // delay/duration/loop 정책을 진행하고 compute tick을 만든다.
    void Start_NextLoop();                          // loop-local spawn/burst state를 초기화하고 다음 loop로 넘어간다.
    float Resolve_CurrentLoopDelay() const;         // 현재 loop에 적용할 delay를 반환한다.
    float Resolve_Duration() const;                 // 0 이하 duration으로 인한 무한 루프를 막는 안전 duration을 반환한다.
    bool Has_NextLoop() const;                      // loopCount 정책상 다음 loop가 있는지 확인한다.
    bool Can_SubmitRender() const;                  // delay/completed kill 정책상 render 제출 가능한지 확인한다.

public:
    static Shared<ComputeSpriteEmitter> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
