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
    HRESULT Reset_ForEffectReplay() override;                                   // effect replay 시 generated path와 재생 상태를 초기화한다.
    bool Is_EffectFinished() const override;                                    // playback loop 완료 여부를 반환한다.
    bool Try_Get_BlendSortWorldPosition(Vec3& outWorldPosition) const override; // beam 중간점을 blend sort 기준점으로 제공한다.
    EffectSortPolicy Get_BlendSortPolicy() const override;                      // Required sort policy를 반환한다.
    int32 Get_BlendSortLayer() const override;                                  // Required sort layer를 반환한다.
    float Get_BlendSortBias() const override;                                   // Required sort bias를 반환한다.

private:
    enum class PlaybackState : uint8
    {
        Delayed,
        Playing,
        Completed,
    };

    struct PathSample
    {
        Vec3 position{};  // generated beam path의 world position sample이다.
        float age{};      // presentation life 평가용 sample age다.
        float distance{}; // beam 시작점 기준 누적 거리다.
    };

    struct SegmentPayload
    {
        Vec4 previousPosition{};              // compute strip tangent용 이전 sample이다.
        Vec4 currentPosition{};               // segment 시작 sample이다.
        Vec4 nextPosition{};                  // segment 끝 sample이다.
        Vec4 nextNextPosition{};              // compute strip tangent용 다음다음 sample이다.
        Vec4 currentSampleParams{};           // x age, y lifetime, z distance, w widthScale.
        Vec4 nextSampleParams{};              // x age, y lifetime, z distance, w widthScale.
        Vec4 startColor{};                    // segment 시작 색이다.
        Vec4 endColor{};                      // segment 끝 색이다.
        Vec4 subUVRect{ 0.f, 0.f, 1.f, 1.f }; // 현재 SubUV rect다.
    };

    struct BeamInstanceVertex
    {
        Vec4 previousPosition{}; // ribbon shader와 동일한 instance layout이다.
        Vec4 currentPosition{};
        Vec4 nextPosition{};
        Vec4 nextNextPosition{};
        Vec2 lifeTime{};
        Vec4 startColor{};
        Vec4 endColor{};
        Vec4 subUVRect{};
        Vec4 segmentParams{};
    };

    struct BeamComputeParams
    {
        uint32 segmentCount{};      // compute로 낮출 segment 수다.
        uint32 renderAxis{};        // ribbon strip shader의 axis mode다.
        float tilingDistance{};     // texture V tiling 기준 거리다.
        float visibleLength{ 1.f }; // 전체 beam visible length다.
        Vec4 axisFallback{};        // axis fallback 값이다.
        Vec4 fadeParams{};          // x auto life fade, y max length, z tail fade length, w reserved.
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
    static constexpr auto kBeamShaderId{ L"Shader_EffectBeam" };                     // beam 전용 좌우 UV 보정 shader다.
    static constexpr auto kBeamDistortionShaderId{ L"Shader_EffectBeamDistortion" }; // distortion beam 전용 shader다
    static constexpr auto kBeamComputeShaderId{ L"ComputeShader_RibbonStrip" };      // segment payload를 instance로 낮추는 compute shader다.
    static constexpr auto kFallbackTexturePath{ "Effects/Textures/Shared/DefaultTexture.dds" };
    static constexpr auto kNeutralFlowTexturePath{ "Effects/Textures/Shared/DefaultFlowMap.dds" };
    static constexpr uint32 kThreadCountX{ 64 }; // compute dispatch thread group 크기다.

private:
    ComputeBeamEmitterDesc _desc{}; // clone 시점에 받은 Beam runtime 설정이다.
    PlaybackState _playbackState{ PlaybackState::Delayed };
    float _loopElapsedTime{};
    uint32 _loopIndex{};
    float _materialElapsedTime{};
    float _visibleBeamLength{ 1.f };
    uint32 _drawSegmentCount{};
    uint32 _randomSubUVFrameIndex{};
    vector<PathSample> _pathSamples{}; // 이번 replay에서 생성된 Beam path sample이다.

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
    void Reset_PlaybackRuntime();                                              // playback 시간/loop 상태를 초기화한다.
    void Advance_Playback(float timeDelta);                                    // delay/play/completed 상태를 전진시킨다.
    void Rebuild_PathSamples();                                                // desc의 local start/end 또는 direction/length로 world sample을 다시 만든다.
    Vec3 Resolve_LocalEnd() const;                                             // endpoint mode에 따라 local end를 계산한다.
    Vec3 Transform_LocalPoint(const Vec3& localPoint) const;                   // emitter local point를 world point로 변환한다.
    Vec3 Evaluate_NoiseOffset(uint32 sampleIndex, const Vec3& tangent) const;  // seed 기반 jitter offset을 만든다.
    HRESULT Append_SegmentPayloads(vector<SegmentPayload>& outPayloads) const; // path sample을 compute input payload로 낮춘다.
    bool Has_RenderablePath() const;                                           // 렌더 가능한 path segment가 있는지 확인한다.

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
    static Shared<ComputeGeneratedBeamEmitter> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
