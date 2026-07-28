#pragma once
#include "Client_Defines.h"
#include "EffectEmitter.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(Engine)
class ComputeShaderCom;
class ComputeStructuredBuffer;
class ShaderCom;
class Texture;
NS_END

NS_BEGIN(Client)

class ComputeGeneratedBeamEmitter final : public EffectEmitter
{
    GENERATED_GAMEOBJECT(ComputeGeneratedBeamEmitter, ObjectType::EffectEmitter);

public:
    ComputeGeneratedBeamEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ComputeGeneratedBeamEmitter(const ComputeGeneratedBeamEmitter& prototype);
    ~ComputeGeneratedBeamEmitter() override;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;

public:
    HRESULT Reset_ForEffectReplay() override;
    bool Is_EffectFinished() const override;
    bool Try_Get_BlendSortWorldPosition(Vec3& outWorldPosition) const override;
    EffectSortPolicy Get_BlendSortPolicy() const override;
    int32 Get_BlendSortLayer() const override;
    float Get_BlendSortBias() const override;

private: //## Types::PlaybackRuntime
    enum class PlaybackState : uint8
    {
        Delayed,
        Playing,
        Completed,
    };

    struct PathSample
    {
        Vec3 position{};
        float age{};
        float distance{};
    };

    struct PathEntry
    {
        vector<PathSample> samples{};
        uint32 stripIndex{};
        float widthScale{ 1.f };
    };

    struct SegmentPayload
    {
        Vec4 previousPosition{};
        Vec4 currentPosition{};
        Vec4 nextPosition{};
        Vec4 nextNextPosition{};
        Vec4 currentSampleParams{};
        Vec4 nextSampleParams{};
        Vec4 startColor{};
        Vec4 endColor{};
        Vec4 subUVRect{ 0.f, 0.f, 1.f, 1.f };
        Vec4 coreColorRgb{ 1.f, 0.85f, 0.45f, 0.f };
    };

    struct BeamInstanceVertex
    {
        Vec4 previousPosition{};
        Vec4 currentPosition{};
        Vec4 nextPosition{};
        Vec4 nextNextPosition{};
        Vec2 lifeTime{};
        Vec4 startColor{};
        Vec4 endColor{};
        Vec4 subUVRect{};
        Vec4 segmentParams{};
        Vec4 coreColorRgb{ 1.f, 0.85f, 0.45f, 0.f };
    };

    struct BeamComputeParams
    {
        uint32 segmentCount{};
        uint32 renderAxis{};
        float tilingDistance{};
        float visibleLength{ 1.f };
        Vec4 axisFallback{};
        Vec4 fadeParams{};
    };

    struct DrawIndexedInstancedIndirectArgs
    {
        uint32 indexCountPerInstance{};
        uint32 instanceCount{};
        uint32 startIndexLocation{};
        int32 baseVertexLocation{};
        uint32 startInstanceLocation{};
    };

private: //## Static::Runtime
    static constexpr auto kBeamShaderId{ L"Shader_EffectBeam" };
    static constexpr auto kBeamDistortionShaderId{ L"Shader_EffectBeamDistortion" };
    static constexpr auto kBeamComputeShaderId{ L"ComputeShader_RibbonStrip" };
    static constexpr auto kFallbackTexturePath{ "Effects/Textures/Shared/DefaultTexture.dds" };
    static constexpr auto kNeutralFlowTexturePath{ "Effects/Textures/Shared/DefaultFlowMap.dds" };
    static constexpr uint32 kThreadCountX{ 64 };

private: //## Data::RuntimeDesc
    ComputeBeamEmitterDesc _desc{};

private: //## Data::PlaybackRuntime
    PlaybackState _playbackState{ PlaybackState::Delayed };
    float _loopElapsedTime{};
    uint32 _loopIndex{};
    float _materialElapsedTime{};
    float _sampledVisualLife{ 1.f };
    float _visibleBeamLength{ 1.f };
    uint32 _drawSegmentCount{};
    uint32 _randomSubUVFrameIndex{};
    vector<PathEntry> _pathEntries{};

private: //## Data::Components
    Shared<ShaderCom> _shader{};
    Shared<ComputeShaderCom> _computeShader{};
    Shared<Texture> _mainTexture{};
    Shared<Texture> _noiseTexture{};
    Shared<Texture> _maskTexture{};
    Shared<Texture> _flowTexture{};
    ComPtr<Buffer> _pointVB{};
    ComPtr<Buffer> _indexBuffer{};
    ComPtr<Buffer> _instanceBuffer{};
    ComPtr<Buffer> _computeConstantBuffer{};
    ComPtr<Buffer> _indirectArgsBuffer{};
    Shared<ComputeStructuredBuffer> _sampleInput{};
    Shared<ComputeStructuredBuffer> _computeOutput{};
    Shared<ComputeStructuredBuffer> _computeArgsOutput{};

private: //## Helper::PlaybackRuntime
    void Reset_PlaybackRuntime();
    void Advance_Playback(float timeDelta);
    void Rebuild_PathSamples();
    Vec3 Resolve_LocalEnd(uint32 stripIndex) const;
    Vec3 Transform_LocalPoint(const Vec3& localPoint) const;
    Vec3 Evaluate_NoiseOffset(uint32 stripIndex, uint32 sampleIndex, const Vec3& tangent) const;
    void Append_BranchPathEntries(const PathEntry& parentPath, uint32 stripIndex, float activeElapsedTime);
    HRESULT Append_SegmentPayloads(vector<SegmentPayload>& outPayloads);
    bool Has_RenderablePath() const;
    void Start_NextLoop();
    void Resample_VisualLife();
    float Resolve_CurrentLoopDelay() const;
    float Resolve_Duration() const;
    float Resolve_VisualLife() const;
    float Resolve_ActiveElapsedTime(float duration) const;
    bool Has_NextLoop() const;

private: //## Helper::Setup
    HRESULT Ready_Components();
    HRESULT Ready_Texture();
    HRESULT Ready_NoiseTexture();
    HRESULT Ready_MaskTexture();
    HRESULT Ready_FlowTexture();
    HRESULT Ready_DrawBuffers();
    HRESULT Ready_ComputeBuffers();

private: //## Helper::Render
    HRESULT Dispatch_Compute();
    HRESULT Update_ComputeInput();
    HRESULT Update_ComputeConstants();
    HRESULT Reset_IndirectArgs();
    HRESULT Copy_ComputeOutput();
    HRESULT Bind_ShaderResources();
    HRESULT Bind_MaterialResources();
    HRESULT Bind_DistortionResources();
    bool Can_SubmitRender() const;
    uint32 Compute_MaxRenderSegmentCount() const;
    bool Is_DistortionFamily() const;
    RenderGroup Resolve_RenderGroup() const;
    const wchar_t* Resolve_ShaderId() const;
    uint32 Resolve_ShaderPassIndex() const;
    int Resolve_OpacitySourceIndex() const;
    Vec4 Evaluate_ColorOverLife(float lifeProgress) const;
    float Evaluate_WidthScaleByLife(float lifeProgress) const;
    Vec3 Evaluate_BeamEnvelopeOverLife(float lifeProgress) const;
    static PathSample Interpolate_PathSample(const PathSample& left, const PathSample& right, float targetDistance);
    uint32 Evaluate_SubUVFrameIndex(uint32 stripIndex) const;
    Vec4 Resolve_SubUVRect(uint32 frameIndex) const;
    uint32 Resolve_RandomSubUVFrameIndex(uint32 stripIndex) const;
    uint32 Resolve_FpsSubUVFramePhaseOffset(uint32 stripIndex, uint32 rangeCount) const;
    void Resample_RandomSubUVFrameIndex();
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

public:
    static Shared<ComputeGeneratedBeamEmitter> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
