#pragma once
#include "Client_Defines.h"
#include "EffectEmitter.h"
#include "EffectRuntime_Types.h"
#include "Particle_Types.h"

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
    HRESULT Reset_ForEffectReplay() override; // effect root 재사용 시 trail history와 draw runtime을 초기화한다.
    bool Is_EffectFinished() const override;  // trail은 일단 자동 완료가 아니라 명시 Stop/release를 따른다.
    void Bind_TrailSampleProvider(const Weak<IEffectTrailSampleProvider>& provider) override;
    // acquire 시점 owner가 준 trail sample provider를 현재 runtime desc에 다시 바인딩한다.

private:
    struct TrailHistorySample
    {
        EffectTrailSample sample{}; // provider에서 받은 base/tip world sample이다.
        float age{};                // sample이 누적된 뒤 흐른 시간이다.
    };

    struct TrailRenderSample
    {
        EffectTrailSample sample{}; // render 직전에 보간된 base/tip world sample이다.
        float age{};                // 보간된 sample의 age다.
        float distanceFromHead{};   // head sample 기준 누적 trail 거리다. UV 거리 계산에 사용한다.
    };

    struct TrailSamplePayload
    {
        Vec4 baseWorldPosition{}; // compute shader가 읽을 trail base world position이다.
        Vec4 tipWorldPosition{};  // compute shader가 읽을 trail tip world position이다.
        Vec4 sampleParams{};      // x: sample age, y: head 기준 누적 거리, zw: reserved.
    };

    struct TrailComputeParams
    {
        uint32 segmentCount{};           // 이번 frame에 유효한 trail segment 수다.
        float segmentLifetime{};         // age fade 기준 lifetime이다.
        float width{};                   // base/tip span에 곱할 strip 폭 비율이다.
        float uvTiling{};                // trail noise/breakup 길이 방향 UV 반복 배율이다.
        float sideFade{};                // 폭 방향 alpha edge 감소 강도다.
        Vec3 padding{};                  // constant buffer 16-byte 정렬 유지용 padding이다.
        Vec4 startColor{};               // InitialColor 모듈에서 낮춘 segment 시작 색상이다.
        Vec4 endColor{};                 // ColorOverLife 모듈에서 낮춘 segment 끝 색상이다.
        Vec4 colorOverLifeCurveParams{}; // x: color key count, y: alpha key count, z/w: enabled flags.
        Vec4 colorOverLifeColorCurveTimes{};
        Vec4 colorOverLifeColorCurveValuesR{};
        Vec4 colorOverLifeColorCurveValuesG{};
        Vec4 colorOverLifeColorCurveValuesB{};
        Vec4 colorOverLifeColorCurveArriveR{};
        Vec4 colorOverLifeColorCurveArriveG{};
        Vec4 colorOverLifeColorCurveArriveB{};
        Vec4 colorOverLifeColorCurveLeaveR{};
        Vec4 colorOverLifeColorCurveLeaveG{};
        Vec4 colorOverLifeColorCurveLeaveB{};
        Vec4 colorOverLifeColorCurveModes{};
        Vec4 colorOverLifeAlphaCurveTimes{};
        Vec4 colorOverLifeAlphaCurveValues{};
        Vec4 colorOverLifeAlphaCurveArrive{};
        Vec4 colorOverLifeAlphaCurveLeave{};
        Vec4 colorOverLifeAlphaCurveModes{};
        Vec4 sizeByLifeParams{};       // x/y: width start/end, z: enabled, w: reserved.
        Vec4 sizeByLifeCurveTimes{};   // SizeByLife scaleOverLife key times다.
        Vec4 sizeByLifeCurveValuesX{}; // SizeByLife scaleOverLife X multiplier 값이다.
        Vec4 sizeByLifeCurveParams{};  // x: key count, y: multiplyX, zw: reserved.
        Vec4 lengthParams{};           // x: maxTrailLength, y: tailFadeLength, zw: reserved.
    };

    struct DrawIndexedInstancedIndirectArgs
    {
        uint32 indexCountPerInstance{}; // DrawIndexedInstancedIndirect의 index count다.
        uint32 instanceCount{};         // compute가 작성하는 실제 draw segment 수다.
        uint32 startIndexLocation{};    // 단일 point index 시작 위치다.
        int32 baseVertexLocation{};     // 단일 point vertex 기준 base offset이다.
        uint32 startInstanceLocation{}; // instance 시작 위치다.
    };

private:
    static constexpr auto kTrailShaderId{ "Shader_EffectTrail" }; // trail segment draw shader id다.
    static constexpr auto kTrailDistortionShaderId{ "Shader_EffectTrailDistortion" }; // SpriteDistortion trail segment draw shader id다.
    static constexpr auto kTrailComputeShaderId{ "ComputeShader_TrailStrip" }; // render sample을 instance payload로 낮추는 compute shader id다.
    static constexpr auto kFallbackTexturePath{ "Effects/Textures/Shared/DefaultTexture.dds" }; // texture 미지정 시 사용할 기본 텍스처다.
    static constexpr uint32 kThreadCountX{ 64 }; // compute shader numthreads.x와 맞춘 dispatch 단위다.
    static constexpr uint32 kMaxCurveSubdivision{ 32 }; // preview 폭주를 막는 interval당 render sample 상한이다.
    static constexpr uint32 kMaxSpawnPerUnitFrameSamples{ 64 }; // 한 frame에서 SpawnPerUnit이 추가할 수 있는 provider history sample 상한이다.
    static constexpr float kSmoothSampleSideWeight{ 0.25f }; // tangent smoothing용 양옆 sample 가중치다.
    static constexpr int kOpacitySourceAlpha{ 0 }; // main texture alpha를 opacity로 사용한다.
    static constexpr int kOpacitySourceRed{ 1 }; // main texture red를 opacity로 사용한다.
    static constexpr int kOpacitySourceLuminance{ 2 }; // main texture luminance를 opacity로 사용한다.

private:
    ComputeTrailEmitterDesc _desc{};       // clone 시점에 받은 trail runtime 설정이다.
    vector<TrailHistorySample> _history{}; // CPU가 보관하는 provider sample history다.
    uint32 _drawSegmentCount{};            // 이번 frame에 그릴 segment 개수다.
    float _materialElapsedTime{};          // material UV scroll 계산에 사용할 emitter-local 누적 시간이다.
    float _visibleTrailLength{ 1.f };      // body/mask UV stretch에 사용할 현재 표시 trail 길이다.

    Shared<ShaderCom> _shader{};               // trail draw shader다.
    Shared<ComputeShaderCom> _computeShader{}; // trail payload 생성 compute shader다.
    Shared<Texture> _mainTexture{};            // trail main texture다.
    Shared<Texture> _noiseTexture{};           // 선택 noise texture. 없으면 material noise를 비활성화한다.
    Shared<Texture> _maskTexture{};            // 선택 mask texture. 없으면 mask alpha 소비를 비활성화한다.
    Shared<Texture> _flowTexture{};            // 선택 distortion flow texture. 없으면 distortion offset을 0으로 처리한다.

    ComPtr<Buffer> _pointVB{};                            // instanced point draw용 단일 vertex buffer다.
    ComPtr<Buffer> _indexBuffer{};                        // instanced point draw용 단일 index buffer다.
    ComPtr<Buffer> _instanceBuffer{};                     // compute 결과를 복사해 draw가 읽는 instance buffer다.
    ComPtr<Buffer> _computeConstantBuffer{};              // compute shader frame parameter buffer다.
    ComPtr<Buffer> _indirectArgsBuffer{};                 // DrawIndexedInstancedIndirect가 읽는 args destination buffer다.
    Shared<ComputeStructuredBuffer> _historyInput{};      // CPU render sample을 compute가 읽는 SRV buffer다.
    Shared<ComputeStructuredBuffer> _computeOutput{};     // compute가 작성하는 TrailInstanceVertex buffer다.
    Shared<ComputeStructuredBuffer> _computeArgsOutput{}; // compute가 작성하는 indirect args source buffer다.

private:
    HRESULT Ready_Components();     // trail draw/compute component와 리소스를 준비한다.
    HRESULT Ready_Texture();        // trail main texture와 fallback texture를 준비한다.
    HRESULT Ready_NoiseTexture();   // 선택 trail noise texture를 준비한다.
    HRESULT Ready_MaskTexture();    // 선택 trail mask texture를 준비한다.
    HRESULT Ready_FlowTexture();    // SpriteDistortion trail flow texture를 준비한다.
    HRESULT Ready_DrawBuffers();    // indirect draw용 point/index/instance buffer를 준비한다.
    HRESULT Ready_ComputeBuffers(); // render sample 입력, instance 출력, indirect args compute buffer를 준비한다.

    void Update_History(float timeDelta); // provider sample을 history에 누적하고 수명/개수를 정리한다.

    uint32 Compute_MaxRenderSegmentCount() const;                             // render-time tessellation을 포함한 최대 segment capacity다.
    void Insert_TrailSamplesForFrame(const EffectTrailSample& trailSample);   // SpawnPerUnit 정책에 따라 provider sample과 보강 sample을 history에 삽입한다.
    void Insert_TrailSample(const EffectTrailSample& trailSample, float age); // provider/render 보강 sample을 history head에 삽입한다.
    void Build_RenderSamples(vector<TrailRenderSample>& outSamples) const;    // 원본 history를 render-time curve sample 배열로 낮춘다.
    EffectTrailSample Build_SmoothedHistorySample(uint32 sampleIndex) const;  // smoothTangent 옵션에 따라 history sample을 완만하게 보정한다.

    HRESULT Dispatch_Compute();              // render sample을 trail instance payload로 낮추는 compute pass를 실행한다.
    HRESULT Update_ComputeInput();           // CPU render sample을 compute 입력 structured buffer에 업로드한다.
    HRESULT Update_ComputeConstants();       // trail compute 상수 버퍼를 현재 desc 값으로 갱신한다.
    HRESULT Reset_IndirectArgs();            // indirect draw args를 draw count 0 상태로 초기화한다.
    HRESULT Copy_ComputeOutput();            // compute 결과 instance/args buffer를 draw용 buffer로 복사한다.
    HRESULT Bind_ShaderResources();          // trail draw shader가 사용할 camera/texture/tint 리소스를 바인딩한다.
    HRESULT Bind_DistortionResources();      // trail distortion field shader가 사용할 flow 파라미터를 바인딩한다.
    bool Is_DistortionFamily() const;        // 현재 trail material family가 distortion 경로인지 확인한다.
    RenderGroup Resolve_RenderGroup() const; // material family에 맞는 render group을 반환한다.
    const char* Resolve_ShaderId() const;    // material family에 맞는 shader prototype id를 반환한다.
    uint32 Resolve_ShaderPassIndex() const;  // 현재 shader에서 사용할 pass index를 반환한다.
    int Resolve_OpacitySourceIndex() const;  // authoring opacitySource 문자열을 shader selector 값으로 변환한다.
    uint32 Resolve_BlendPassIndex() const;   // material blend mode에 맞는 EffectTrail shader pass를 고른다.

public:
    static Shared<ComputeTrailEmitter> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
