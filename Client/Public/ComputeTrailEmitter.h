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

class ComputeTrailEmitter final : public EffectEmitter
{
    GENERATED_GAMEOBJECT(ComputeTrailEmitter, ObjectType::EffectEmitter);

public:
    ComputeTrailEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ComputeTrailEmitter(const ComputeTrailEmitter& prototype);
    ~ComputeTrailEmitter() override;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;

public:
    HRESULT Reset_ForEffectReplay() override;
    bool Is_EffectFinished() const override;
    void Bind_TrailSampleProvider(const Weak<IEffectTrailSampleProvider>& provider) override;

private: //## Types::TrailRuntime
    struct TrailHistorySample
    {
        EffectTrailSample sample{};
        float age{};
        uint32 serial{};
    };

    struct TrailRenderSample
    {
        EffectTrailSample sample{};
        float age{};
        float distanceFromHead{};
        uint32 serial{};
    };

    struct TrailSamplePayload
    {
        Vec4 baseWorldPosition{};
        Vec4 tipWorldPosition{};
        Vec4 sampleParams{};
        Vec4 coreColorRgb{ 1.f, 0.85f, 0.45f, 0.f };
    };

    struct TrailComputeParams
    {
        uint32 segmentCount{};
        float segmentLifetime{};
        float width{};
        float uvTiling{};
        float sideFade{};
        Vec3 padding{};
        Vec4 startColor{};
        Vec4 endColor{};
        Vec4 colorOverLifeCurveParams{};
        Vec4 colorOverLifeColorCurveTimes{};
        Vec4 colorOverLifeColorCurveTimesBlock1{};
        Vec4 colorOverLifeColorCurveValuesR{};
        Vec4 colorOverLifeColorCurveValuesRBlock1{};
        Vec4 colorOverLifeColorCurveValuesG{};
        Vec4 colorOverLifeColorCurveValuesGBlock1{};
        Vec4 colorOverLifeColorCurveValuesB{};
        Vec4 colorOverLifeColorCurveValuesBBlock1{};
        Vec4 colorOverLifeColorCurveArriveR{};
        Vec4 colorOverLifeColorCurveArriveRBlock1{};
        Vec4 colorOverLifeColorCurveArriveG{};
        Vec4 colorOverLifeColorCurveArriveGBlock1{};
        Vec4 colorOverLifeColorCurveArriveB{};
        Vec4 colorOverLifeColorCurveArriveBBlock1{};
        Vec4 colorOverLifeColorCurveLeaveR{};
        Vec4 colorOverLifeColorCurveLeaveRBlock1{};
        Vec4 colorOverLifeColorCurveLeaveG{};
        Vec4 colorOverLifeColorCurveLeaveGBlock1{};
        Vec4 colorOverLifeColorCurveLeaveB{};
        Vec4 colorOverLifeColorCurveLeaveBBlock1{};
        Vec4 colorOverLifeColorCurveModes{};
        Vec4 colorOverLifeColorCurveModesBlock1{};
        Vec4 colorOverLifeAlphaCurveTimes{};
        Vec4 colorOverLifeAlphaCurveTimesBlock1{};
        Vec4 colorOverLifeAlphaCurveValues{};
        Vec4 colorOverLifeAlphaCurveValuesBlock1{};
        Vec4 colorOverLifeAlphaCurveArrive{};
        Vec4 colorOverLifeAlphaCurveArriveBlock1{};
        Vec4 colorOverLifeAlphaCurveLeave{};
        Vec4 colorOverLifeAlphaCurveLeaveBlock1{};
        Vec4 colorOverLifeAlphaCurveModes{};
        Vec4 colorOverLifeAlphaCurveModesBlock1{};
        Vec4 sizeByLifeParams{};
        Vec4 sizeByLifeCurveTimes{};
        Vec4 sizeByLifeCurveTimesBlock1{};
        Vec4 sizeByLifeCurveValuesX{};
        Vec4 sizeByLifeCurveValuesXBlock1{};
        Vec4 sizeByLifeCurveArriveTangentsX{};
        Vec4 sizeByLifeCurveArriveTangentsXBlock1{};
        Vec4 sizeByLifeCurveLeaveTangentsX{};
        Vec4 sizeByLifeCurveLeaveTangentsXBlock1{};
        Vec4 sizeByLifeCurveModes{};
        Vec4 sizeByLifeCurveModesBlock1{};
        Vec4 sizeByLifeCurveParams{};
        Vec4 lengthParams{};
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
    static constexpr auto kTrailShaderId{ "Shader_EffectTrail" };
    static constexpr auto kTrailDistortionShaderId{ "Shader_EffectTrailDistortion" };
    static constexpr auto kTrailComputeShaderId{ "ComputeShader_TrailStrip" };
    static constexpr auto kFallbackTexturePath{ "Effects/Textures/Shared/DefaultTexture.dds" };
    static constexpr uint32 kThreadCountX{ 64 };
    static constexpr uint32 kMaxCurveSubdivision{ 32 };
    static constexpr uint32 kMaxSpawnPerUnitFrameSamples{ 64 };
    static constexpr float kSmoothSampleSideWeight{ 0.25f };
    static constexpr int kOpacitySourceAlpha{ 0 };
    static constexpr int kOpacitySourceRed{ 1 };
    static constexpr int kOpacitySourceLuminance{ 2 };

private: //## Data::RuntimeDesc
    ComputeTrailEmitterDesc _desc{};

private: //## Data::TrailRuntime
    vector<TrailHistorySample> _history{};
    uint32 _drawSegmentCount{};
    float _materialElapsedTime{};
    float _visibleTrailLength{ 1.f };
    uint32 _sampleSerialCounter{};

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
    Shared<ComputeStructuredBuffer> _historyInput{};
    Shared<ComputeStructuredBuffer> _computeOutput{};
    Shared<ComputeStructuredBuffer> _computeArgsOutput{};

private: //## Helper::Setup
    HRESULT Ready_Components();
    HRESULT Ready_Texture();
    HRESULT Ready_NoiseTexture();
    HRESULT Ready_MaskTexture();
    HRESULT Ready_FlowTexture();
    HRESULT Ready_DrawBuffers();
    HRESULT Ready_ComputeBuffers();

private: //## Helper::TrailRuntime
    void Update_History(float timeDelta);
    uint32 Count_VisibleHistorySamples() const;
    void Prune_HistoryForVisibleControl();
    uint32 Compute_MaxRenderSegmentCount() const;
    void Insert_TrailSamplesForFrame(const EffectTrailSample& trailSample);
    void Insert_TrailSample(const EffectTrailSample& trailSample, float age);
    void Build_RenderSamples(vector<TrailRenderSample>& outSamples) const;
    EffectTrailSample Build_SmoothedHistorySample(uint32 sampleIndex) const;

private: //## Helper::Render
    HRESULT Dispatch_Compute();
    HRESULT Update_ComputeInput();
    HRESULT Update_ComputeConstants();
    HRESULT Reset_IndirectArgs();
    HRESULT Copy_ComputeOutput();
    HRESULT Bind_ShaderResources();
    HRESULT Bind_DistortionResources();
    void Resample_SegmentLifetime();
    bool Is_DistortionFamily() const;
    RenderGroup Resolve_RenderGroup() const;
    const char* Resolve_ShaderId() const;
    uint32 Resolve_ShaderPassIndex() const;
    int Resolve_OpacitySourceIndex() const;
    uint32 Resolve_BlendPassIndex() const;

public:
    static Shared<ComputeTrailEmitter> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
