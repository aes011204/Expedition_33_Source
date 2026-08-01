#pragma once
#include "Client_Defines.h"
#include "EffectEmitter.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(Engine)
class ModelCom;
class ShaderCom;
class Texture;
NS_END

NS_BEGIN(Client)

class MeshEmitter final : public EffectEmitter
{
    GENERATED_GAMEOBJECT(MeshEmitter, ObjectType::EffectEmitter);

public:
    MeshEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MeshEmitter(const MeshEmitter& prototype);
    ~MeshEmitter() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;

public:
    HRESULT Reset_ForEffectReplay() override;
    bool Can_ReloadDefinition(const EffectEmitterDefinition& emitterDefinition) const override;
    HRESULT Reload_Definition(const EffectEmitterDefinition& emitterDefinition) override;
    bool Is_EffectFinished() const override;
    bool Collect_FollowerSourcePoints(vector<EffectFollowerSourcePoint>& outPoints, uint32 maxPointCount) const override;
    bool Try_Get_BlendSortWorldPosition(Vec3& outWorldPosition) const override;
    EffectSortPolicy Get_BlendSortPolicy() const override;
    int32 Get_BlendSortLayer() const override;
    float Get_BlendSortBias() const override;

protected: //## Hook::TransformSync
    void On_EffectTransformSynced() override;

private: //## Types::PlaybackRuntime
    enum class PlaybackState : uint8
    {
        Delayed,
        Playing,
        Completed,
    };

    struct MeshInstanceVertex
    {
        Matrix world{};
        Vec4 color{ 1.f, 1.f, 1.f, 1.f };
        Vec2 lifeTime{};
        Vec2 padding{};
        Vec4 coreColorRgb{ 1.f, 0.85f, 0.45f, 0.f };
    };

    struct MeshParticleState
    {
        bool active{ false };
        float age{};
        float lifeMax{ 1.f };
        Vec3 position{};
        Vec3 velocity{};
        Vec3 initialVelocity{};
        Vec3 initialRadialVelocity{};
        Vec3 velocityCone{};
        Vec3 sourceMotionVelocity{};
        Vec3 accelerationIntegratedVelocity{};
        Vec3 acceleration{};
        float drag{};
        Vec3 baseScale{ 1.f, 1.f, 1.f };
        Vec3 spawnOffset{};
        Vec3 spawnPosition{};
        Quat spawnTransformRotation{ Quat::Identity };
        Quat spawnOrientation{ Quat::Identity };
        bool spawnOrientationEnabled{ false };
        Vec3 rotationRadians{};
        Quat rotationQuat{ Quat::Identity };
        Vec3 angularVelocityRadians{};
        float directionAlignRandomDelay{ 0.f };
        float directionAlignWeightScale{ 1.f };
        Vec4 startColor{ 1.f, 1.f, 1.f, 1.f };
        Vec4 endColor{ 1.f, 1.f, 1.f, 1.f };
        Vec3 coreColorRgb{ 1.f, 0.85f, 0.45f };
        uint32 seed{};
    };

    struct PlaneRadialSample
    {
        bool enabled{ false };
        Vec3 localOffset{};
        Vec3 radialDirection{ 1.f, 0.f, 0.f };
        Vec3 tangentCW{ 0.f, -1.f, 0.f };
        Vec3 tangentCCW{ 0.f, 1.f, 0.f };
        Vec3 planeNormal{ 0.f, 0.f, 1.f };
        float sampleAngleRadians{};
    };

    struct SphereRadialSample
    {
        bool enabled{ false };
        Vec3 localOffset{};
        Vec3 radialDirection{ 0.f, 0.f, 1.f };
    };

    struct CylinderRadialSample
    {
        bool enabled{ false };
        Vec3 localOffset{};
        Vec3 radialDirection{ 1.f, 0.f, 0.f };
        Vec3 tangentCW{ 0.f, -1.f, 0.f };
        Vec3 tangentCCW{ 0.f, 1.f, 0.f };
        Vec3 cylinderAxis{ 0.f, 0.f, 1.f };
    };

    struct SourceMotionVelocityContext
    {
        Vec3 sourceVelocity{};
        Vec3 tangent{ 0.f, 0.f, 1.f };
        bool sourceVelocityValid{ false };
    };

private: //## Static::Runtime
    static constexpr auto kEffectMeshShaderId{ "Shader_EffectMesh" };
    static constexpr auto kEffectMeshDistortionShaderId{ "Shader_EffectMeshDistortion" };
    static constexpr auto kEffectMeshGlassShaderId{ "Shader_EffectMeshGlass" };
    static constexpr auto kFallbackTexturePath{ "Effects/Textures/Shared/DefaultTexture.dds" };
    static constexpr int kOpacitySourceAlpha{ 0 };
    static constexpr int kOpacitySourceRed{ 1 };
    static constexpr int kOpacitySourceLuminance{ 2 };

private: //## Data::Components
    Shared<ModelCom> _model{};
    Shared<ShaderCom> _shader{};
    wstring _loadedShaderPrototypeTag{};
    Shared<Texture> _texture{};
    Shared<Texture> _noiseTexture{};
    Shared<Texture> _maskTexture{};
    Shared<Texture> _flowTexture{};
    ComPtr<Buffer> _instanceBuffer{};

private: //## Data::RuntimeDesc
    MeshEmitterDesc _desc{};
    int _opacitySourceIndex{ kOpacitySourceAlpha };
    bool _isReady{ false };
    mutable bool _sphereRadialMeshOrientationFallbackLogged{ false };
    mutable bool _planeRadialMeshOrientationFallbackLogged{ false };
    mutable bool _cylinderRadialMeshOrientationFallbackLogged{ false };
    mutable bool _meshDirectionAlignFallbackLogged{ false };

private: //## Data::ParticleRuntime
    vector<MeshParticleState> _particles{};
    vector<MeshInstanceVertex> _instancePayload{};
    uint32 _activeInstanceCount{};
    uint32 _nextSpawnCursor{};
    uint32 _instanceCapacity{};
    Vec3 _sourceMotionVelocity{};
    Vec3 _sourceMotionTangent{ 0.f, 0.f, 1.f };
    Vec3 _lastSourceMotionCenter{};
    bool _sourceMotionVelocityValid{ false };
    bool _hasLastSourceMotionCenter{ false };

private: //## Data::SpawnRuntime
    vector<bool> _burstExecuted{};
    float _emitterElapsedTime{};
    float _spawnAccumulator{};
    float _sampledSpawnRate{ 10.f };
    float _sampledSpawnRateScale{ 1.f };

private: //## Data::PlaybackRuntime
    PlaybackState _playbackState{ PlaybackState::Delayed };
    float _loopElapsedTime{};
    uint32 _loopIndex{};
    bool _completedKillDispatched{};

private: //## Helper::DefinitionReload
    void Log_DescPayload(const char* context) const;
    void Log_InitializeTiming(
        const char* context,
        double shaderMs,
        double resolveModelMs,
        double modelMs,
        double materialMs,
        double bufferMs,
        double totalMs) const;
    bool Is_CompatibleReloadDesc(const MeshEmitterDesc& desc) const;
    bool Has_MaterialOverride(const MeshEmitterDesc& desc) const;
    HRESULT Apply_DescForReload(const MeshEmitterDesc& desc);

private: //## Helper::Setup
    HRESULT Ready_Components();
    HRESULT Ready_Shader();
    HRESULT Ready_InstanceBuffer();
    HRESULT Apply_EffectMaterialOverride();
    HRESULT Ready_EffectMaterialTextures();
    HRESULT Ready_OptionalTexture(const string& textureGuid, const string& texturePath, Shared<Texture>& outTexture);
    wstring Resolve_ModelPath() const;
    wstring Resolve_MaterialTexturePath(const string& textureGuid, const string& texturePath) const;

private: //## Helper::Render
    HRESULT Bind_EffectMaterialResources();
    HRESULT Bind_ModelMaterialResourcesForMesh(uint32 meshIndex, uint32 materialIndex);
    HRESULT Bind_DistortionResources();
    bool Is_DistortionFamily() const;
    bool Is_MeshGlassFamily() const;
    Matrix Build_InstanceMatrix(const MeshParticleState& particle) const;
    Vec3 Resolve_ParticleWorldPosition(const MeshParticleState& particle) const;
    RenderGroup Resolve_RenderGroup() const;
    uint32 Resolve_ShaderPassIndex() const;
    int Resolve_OpacitySourceIndex() const;

private: //## Helper::PlaybackRuntime
    void Reset_PlaybackRuntime();
    void Advance_Playback(float timeDelta);
    void Start_NextLoop();
    float Resolve_CurrentLoopDelay() const;
    float Resolve_Duration() const;
    bool Has_NextLoop() const;
    bool Can_SubmitRender() const;

private: //## Helper::ParticleRuntime
    uint32 Count_ActiveParticles() const;
    uint32 Consume_SpawnRequest(float timeDelta);
    void Resample_LoopSpawnDistributions();
    float Sample_SpawnDistribution(const Vec2& range, const PointParticleRandomSeedRuntimeDesc& seed, uint32 salt) const;
    void Spawn_Particles(uint32 spawnRequest);
    void Activate_Particle(MeshParticleState& particle, uint32 seed, uint32 particleIndex, uint32 placementIndex, uint32 placementCount);
    void Update_Particles(float timeDelta);
    void Update_InstanceBuffer();

private: //## Helper::SamplingEvaluation
    Vec3 Sample_InitialOffset(
        uint32 seed,
        uint32 particleIndex,
        uint32 placementIndex,
        uint32 placementCount,
        PlaneRadialSample& outPlaneRadialSample,
        SphereRadialSample& outSphereRadialSample,
        CylinderRadialSample& outCylinderRadialSample) const;
    Quat Build_SphereRadialMeshOrientation(const SphereRadialSample& sample, bool& outEnabled) const;
    Quat Build_PlaneRadialMeshOrientation(const PlaneRadialSample& sample, bool& outEnabled) const;
    Quat Build_CylinderMeshOrientation(const CylinderRadialSample& sample, bool& outEnabled) const;
    float Evaluate_DirectionAlignWeight(float lifeProgress, const MeshParticleState& particle) const;
    Quat Build_MeshDirectionAlignTargetOrientation(const MeshParticleState& particle, const Vec3& visualPosition, bool& outEnabled) const;
    void Sample_InitialVelocityChannels(
        MeshParticleState& particle,
        const Vec3& spawnPosition,
        const PlaneRadialSample& planeRadialSample,
        const SourceMotionVelocityContext& sourceMotionContext,
        uint32 seed) const;
    void Update_SourceMotionVelocity(float timeDelta);
    Vec3 Evaluate_OrbitOffset(const MeshParticleState& particle, float lifeProgress) const;
    Vec3 Sample_Acceleration(uint32 seed) const;
    float Sample_Drag(uint32 seed) const;
    Vec4 Sample_Color(
        const Vec4& minColor,
        const Vec4& maxColor,
        const PointParticleRandomSeedRuntimeDesc& colorSeed,
        const PointParticleRandomSeedRuntimeDesc& alphaSeed,
        uint32 seed) const;
    Vec3 Sample_InitialScale(uint32 seed) const;
    Vec3 Sample_InitialRotation(uint32 seed) const;
    Vec3 Sample_InitialAngularVelocity(uint32 seed) const;
    float Evaluate_VelocityScaleByLifeChannel(const PointParticleFloatCurveRuntimeDesc& curve, float lifeProgress) const;
    Vec3 Evaluate_ScaledVelocityChannels(const MeshParticleState& particle, float lifeProgress) const;
    Vec3 Evaluate_AccelerationByLife(const MeshParticleState& particle, float lifeProgress) const;
    Vec3 Evaluate_ScaleByLife(float lifeProgress) const;
    Vec3 Evaluate_RotationByLife(float lifeProgress) const;
    Vec3 Evaluate_AngularVelocityScaleByLife(float lifeProgress) const;
    Vec4 Evaluate_ColorOverLife(const MeshParticleState& particle, float lifeProgress) const;
    float Evaluate_CompactCurve(
        float lifeProgress,
        const Vec4& times,
        const Vec4& timesBlock1,
        const Vec4& values,
        const Vec4& valuesBlock1,
        uint32 keyCount,
        float fallbackValue) const;
    float Evaluate_CompactCurve(
        float lifeProgress,
        const Vec4& times,
        const Vec4& timesBlock1,
        const Vec4& values,
        const Vec4& valuesBlock1,
        const Vec4& arriveTangents,
        const Vec4& arriveTangentsBlock1,
        const Vec4& leaveTangents,
        const Vec4& leaveTangentsBlock1,
        const Vec4& modes,
        const Vec4& modesBlock1,
        uint32 keyCount,
        float fallbackValue) const;
    Vec3 Evaluate_MeshVector3Curve(const MeshVector3CurveRuntimeDesc& desc, float lifeProgress, const Vec3& fallbackValue) const;
    Vec3 Resolve_LocalVector(const Vec3& localVector) const;
    Vec3 Resolve_WorldVectorToSimulation(const Vec3& worldVector) const;
    Vec3 Resolve_LocalPoint(const Vec3& localPoint) const;
    Vec3 Resolve_WorldPointToSimulation(const Vec3& worldPoint) const;
    float Random01(uint32 seed) const;
    float RandomRange(float minValue, float maxValue, uint32 seed) const;

public:
    static Shared<MeshEmitter> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
