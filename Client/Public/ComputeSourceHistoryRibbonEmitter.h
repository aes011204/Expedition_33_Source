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

class ComputeSourceHistoryRibbonEmitter final : public EffectEmitter
{
    GENERATED_GAMEOBJECT(ComputeSourceHistoryRibbonEmitter, ObjectType::EffectEmitter);

public:
    ComputeSourceHistoryRibbonEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ComputeSourceHistoryRibbonEmitter(const ComputeSourceHistoryRibbonEmitter& prototype);
    ~ComputeSourceHistoryRibbonEmitter() override;

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

private:
    enum class PlaybackState : uint8
    {
        Delayed,
        Playing,
        Completed,
    };

    struct HistorySample
    {
        Vec3 position{};
        float age{};
        float distance{};
    };

    struct RenderSample
    {
        HistorySample sample{};
        float distanceFromHead{};
    };

    struct SegmentPayload
    {
        Vec4 previousPosition{};
        Vec4 currentPosition{};
        Vec4 nextPosition{};
        Vec4 nextNextPosition{};
        Vec4 currentSampleParams{}; // x: age, y: lifetime, z: distance, w: widthScale.
        Vec4 nextSampleParams{};    // x: age, y: lifetime, z: distance, w: widthScale.
        Vec4 startColor{};
        Vec4 endColor{};
        Vec4 subUVRect{ 0.f, 0.f, 1.f, 1.f };
    };

    struct HistoryLane
    {
        uint32 sourceIndex{};
        vector<HistorySample> history{};
        float sampleIntervalAccumulator{};
        bool activeThisFrame{};
    };

    struct RibbonComputeParams
    {
        uint32 segmentCount{};
        uint32 renderAxis{};
        float tilingDistance{};
        float visibleLength{ 1.f };
        Vec4 axisFallback{};
        Vec4 fadeParams{}; // x: auto life fade, y: max length, z: tail fade length, w: reserved.
    };

    struct RibbonInstanceVertex
    {
        Vec4 previousPosition{}; // xyz: segment 시작점 앞 render sample, w: hasPreviousSample flag다.
        Vec4 currentPosition{};  // segment 시작 sample world position이다.
        Vec4 nextPosition{};     // segment 끝 sample world position이다.
        Vec4 nextNextPosition{}; // xyz: segment 끝점 다음 render sample, w: hasNextNextSample flag다.
        Vec2 lifeTime{};
        Vec4 startColor{};
        Vec4 endColor{};
        Vec4 subUVRect{};
        Vec4 segmentParams{}; // x: lifeProgress, y: currentWidth, z: nextWidth, w: alphaScale.
    };

    struct DrawIndexedInstancedIndirectArgs
    {
        uint32 indexCountPerInstance{};
        uint32 instanceCount{};
        uint32 startIndexLocation{};
        int32 baseVertexLocation{};
        uint32 startInstanceLocation{};
    };

private:
    static constexpr auto kRibbonShaderId{ L"Shader_EffectRibbon" };
    static constexpr auto kRibbonDistortionShaderId{ L"Shader_EffectRibbonDistortion" };
    static constexpr auto kRibbonComputeShaderId{ L"ComputeShader_RibbonStrip" };
    static constexpr auto kFallbackTexturePath{ "Effects/Textures/Shared/DefaultTexture.dds" };
    static constexpr auto kNeutralFlowTexturePath{ "Effects/Textures/Shared/DefaultFlowMap.dds" };
    static constexpr uint32 kThreadCountX{ 64 };
    static constexpr uint32 kMaxDistanceTessellationSubdivision{ 8 };

private:
    ComputeSourceHistoryRibbonEmitterDesc _desc{};
    PlaybackState _playbackState{ PlaybackState::Delayed };
    float _loopElapsedTime{};
    uint32 _loopIndex{};
    vector<HistorySample> _history{};
    float _sampleIntervalAccumulator{};
    vector<HistoryLane> _sourceLanes{};
    float _materialElapsedTime{};
    float _visibleRibbonLength{ 1.f };
    uint32 _drawSegmentCount{};
    uint32 _randomSubUVFrameIndex{};

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

private:
    void Reset_PlaybackRuntime();
    void Advance_Playback(float timeDelta);
    void Update_History(float timeDelta);
    void Update_SelfRootHistory(float timeDelta);
    void Update_ParticleFollowerHistory(float timeDelta);
    bool Should_InsertSample(const vector<HistorySample>& history, float sampleIntervalAccumulator, const Vec3& sourcePosition) const;
    void Insert_HistorySample(vector<HistorySample>& history, float& sampleIntervalAccumulator, const Vec3& sourcePosition);
    void Prune_History(vector<HistorySample>& history);
    void Build_RenderSamples(vector<RenderSample>& outSamples) const;
    void Build_RenderSamples(const vector<HistorySample>& history, vector<RenderSample>& outSamples) const;
    HRESULT Append_SegmentPayloads(const vector<RenderSample>& renderSamples, vector<SegmentPayload>& outPayloads) const;
    bool Has_RenderableHistory() const;
    HistoryLane* Find_HistoryLane(uint32 sourceIndex);
    void Clear_ParticleFollowerHistory();

    HRESULT Ready_Components();
    HRESULT Ready_Texture();
    HRESULT Ready_NoiseTexture();
    HRESULT Ready_MaskTexture();
    HRESULT Ready_FlowTexture();
    HRESULT Ready_DrawBuffers();
    HRESULT Ready_ComputeBuffers();
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

    void Start_NextLoop();
    float Resolve_CurrentLoopDelay() const;
    float Resolve_Duration() const;
    bool Has_NextLoop() const;
    bool Is_DistortionFamily() const;
    RenderGroup Resolve_RenderGroup() const;
    const wchar_t* Resolve_ShaderId() const;
    uint32 Resolve_ShaderPassIndex() const;
    int Resolve_OpacitySourceIndex() const;
    Vec3 Resolve_SourcePosition() const;
    Vec3 Resolve_PreviewMotionOffset() const;
    Vec4 Evaluate_ColorOverLife(float lifeProgress) const;
    float Evaluate_WidthScaleByLife(float lifeProgress) const;
    uint32 Evaluate_SubUVFrameIndex() const;
    Vec4 Resolve_SubUVRect(uint32 frameIndex) const;
    void Resample_RandomSubUVFrameIndex();
    float Evaluate_CompactCurve(
        float lifeProgress,
        const Vec4& times,
        const Vec4& timesBlock1,
        const Vec4& values,
        const Vec4& valuesBlock1,
        uint32 keyCount,
        float fallbackValue) const;

public:
    static Shared<ComputeSourceHistoryRibbonEmitter> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
