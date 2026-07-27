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
    HRESULT Reset_ForEffectReplay() override;
    void Set_MaterialRevealOverride(float alphaMultiplier, float alphaErosion) override;
    void Set_MaterialTintOverride(const Vec4& targetTint, float strength) override;
    bool Is_EffectFinished() const override;
    bool Collect_FollowerSourcePoints(vector<EffectFollowerSourcePoint>& outPoints, uint32 maxPointCount) const override;
    bool Try_Get_BlendSortWorldPosition(Vec3& outWorldPosition) const override;
    EffectSortPolicy Get_BlendSortPolicy() const override;
    int32 Get_BlendSortLayer() const override;
    float Get_BlendSortBias() const override;

public: //## Debug::EditorSession
    static void Set_InfluenceOutlineDebugEnabled(bool enabled);
    static bool Is_InfluenceOutlineDebugEnabled();

protected: //## Hook::TransformSync
    void On_EffectTransformSynced() override;

private: //## Types::PlaybackRuntime
    enum class PlaybackState : uint8
    {
        Delayed,
        Playing,
        Completed,
    };

    struct PlaybackTick
    {
        float computeDeltaTime{};
        float lifetimeSamplePhase{};
        uint32 spawnRequest{};
        uint32 spawnSerialBase{};
        bool killActiveParticles{};
    };

private: //## Static::Runtime
    static constexpr auto kFallbackTexturePath{ "Effects/Textures/Shared/DefaultTexture.dds" };
    static constexpr auto kEffectSpriteShaderId{ "Shader_EffectSprite" };
    static constexpr auto kEffectDistortionSpriteShaderId{ "Shader_EffectDistortionSprite" };
    static constexpr int kOpacitySourceAlpha{ 0 };
    static constexpr int kOpacitySourceRed{ 1 };
    static constexpr int kOpacitySourceLuminance{ 2 };
    static constexpr int kOpacitySourceOne{ 3 };
    static constexpr float kInfluenceOutlinePixelWidthScale{ 2.f };
    inline static bool _isInfluenceOutlineDebugEnabled{ false };

private: //## Data::Components
    Shared<ShaderCom> _shader{};
    Shared<Texture> _texture{};
    Shared<Texture> _noiseTexture{};
    Shared<Texture> _maskTexture{};
    Shared<Texture> _flowTexture{};
    Shared<VIBufferCom_ComputePointParticle> _viBuffer{};

private: //## Data::RuntimeDesc
    ComputeSpriteEmitterDesc _desc{};
    int _opacitySourceIndex{ kOpacitySourceAlpha };
    float _materialRevealAlphaMultiplier = 1.f;
    float _materialRevealAlphaErosion = 0.f;
    Vec4 _materialTintOverrideTarget{ 1.f, 1.f, 1.f, 1.f };
    float _materialTintOverrideStrength = 0.f;
    Vec3 _runtimeDescWorldScale{ Vec3::One };
    bool _hasRuntimeDescWorldScale{ false };

private: //## Data::SpawnRuntime
    vector<bool> _burstExecuted{};
    float _emitterElapsedTime{};
    float _spawnAccumulator{};
    uint32 _spawnSerialCounter{};
    float _sampledSpawnRate{ 10.f };
    float _sampledSpawnRateScale{ 1.f };

private: //## Data::PlaybackRuntime
    PlaybackState _playbackState{ PlaybackState::Delayed };
    float _loopElapsedTime{};
    uint32 _loopIndex{};
    bool _completedKillDispatched{};

private: //## Helper::Setup
    HRESULT Ready_Components();
    HRESULT Ready_TextureComponent(string& outResolvedTexturePath);
    HRESULT Ready_ShaderComponent();
    HRESULT Ready_NoiseTextureComponent(const string& resolvedTexturePath);
    HRESULT Ready_MaskTextureComponent(const string& resolvedTexturePath);
    HRESULT Ready_FlowTextureComponent(const string& resolvedTexturePath);

private: //## Helper::Render
    HRESULT Bind_ShaderResources();
    HRESULT Bind_MaterialResources();
    HRESULT Bind_DistortionResources();
    float Resolve_MaterialScalarEmitterPhase() const;
    bool Is_DistortionFamily() const;
    RenderGroup Resolve_RenderGroup() const;
    const char* Resolve_ShaderId() const;
    uint32 Resolve_ShaderPassIndex() const;
    int Resolve_OpacitySourceIndex() const;
    uint32 Resolve_BlendPassIndex() const;

private: //## Helper::RuntimeDesc
    Vec3 Resolve_RuntimeWorldScale() const;
    ComputePointParticleDesc Make_ComputePointParticleDesc() const;
    void Update_RuntimeDescForCurrentScale(bool forceUpdate);
    void Sync_WorldCenterToComputeBuffer();

private: //## Helper::SpawnRuntime
    uint32 Consume_SpawnRequest(float timeDelta);
    void Resample_LoopSpawnDistributions();
    float Sample_SpawnDistribution(const Vec2& range, const PointParticleRandomSeedRuntimeDesc& seed, uint32 salt) const;

private: //## Helper::PlaybackRuntime
    void Reset_PlaybackRuntime();
    PlaybackTick Advance_Playback(float timeDelta);
    void Start_NextLoop();
    float Resolve_CurrentLoopDelay() const;
    float Resolve_Duration() const;
    bool Has_NextLoop() const;
    bool Can_SubmitRender() const;

public:
    static Shared<ComputeSpriteEmitter> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
