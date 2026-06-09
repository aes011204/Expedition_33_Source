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

class ComputeRibbonEmitter final : public EffectEmitter
{
    GENERATED_GAMEOBJECT(ComputeRibbonEmitter, ObjectType::EffectEmitter);

public:
    ComputeRibbonEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ComputeRibbonEmitter(const ComputeRibbonEmitter& prototype);
    ~ComputeRibbonEmitter() override;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;

public:
    HRESULT Reset_ForEffectReplay() override;                                   // effect root 재사용 시 ribbon playback runtime을 초기화한다.
    bool Is_EffectFinished() const override;                                    // ribbon playback이 마지막 loop를 끝냈는지 반환한다.
    bool Try_Get_BlendSortWorldPosition(Vec3& outWorldPosition) const override; // root/local sync가 반영된 ribbon 대표 위치를 반환한다.
    EffectSortPolicy Get_BlendSortPolicy() const override;                      // Required Sort Policy authoring 값을 반환한다.
    int32 Get_BlendSortLayer() const override;                                  // Required Sort Layer authoring 값을 반환한다.
    float Get_BlendSortBias() const override;                                   // Required Sort Bias authoring 값을 Blend 정렬 보정값으로 반환한다.

private:
    enum class PlaybackState : uint8
    {
        Delayed,
        Playing,
        Completed,
    };

    struct RibbonSample
    {
        Vec3 position{};       // Ribbon self-particle chain의 world 위치다.
        Vec3 velocity{};       // sample 이동 갱신에 사용할 world velocity다.
        Vec3 acceleration{};   // sample별 acceleration 분포 평가 결과다.
        float drag{};          // sample별 drag 분포 평가 결과다.
        float age{};           // sample 생성 이후 누적 시간이다.
        float lifetime{ 1.f }; // sample별 최대 생존 시간이다.
        float distance{};      // head 기준 누적 거리 UV 계산값이다.
        Vec4 startColor{};     // InitialColor에서 평가된 시작 색이다.
        Vec4 endColor{};       // ColorOverLife fallback 끝 색이다.
        uint32 seed{};         // sample별 deterministic random sample seed다.
    };

    struct PlaneRadialSample
    {
        bool enabled{ false }; // PlaneRadialLocation이 실제 sample frame을 만들었는지 나타낸다.
        Vec3 localOffset{};    // 기존 location module과 더할 emitter-local offset이다.
        Vec3 radialDirection{ 1.f, 0.f, 0.f };
    };

    struct RibbonRenderSample
    {
        RibbonSample sample{};
        float distanceFromHead{};
    };

    struct RibbonSamplePayload
    {
        Vec4 currentWorldPosition{}; // compute 입력 sample 위치다.
        Vec4 sampleParams{};         // x: age, y: lifetime, z: distance, w: widthScale.
        Vec4 startColor{};           // compute 출력 색 계산용 시작 색이다.
        Vec4 endColor{};             // compute 출력 색 계산용 끝 색이다.
    };

    struct RibbonComputeParams
    {
        uint32 segmentCount{};      // 이번 frame에 그릴 ribbon segment 수다.
        uint32 renderAxis{};        // EffectRibbonRenderAxis 값이다.
        float tilingDistance{};     // 0 이하면 stretch, 양수면 길이 기준 반복이다.
        float visibleLength{ 1.f }; // stretch UV 계산에 사용할 현재 chain 길이다.
        Vec4 axisFallback{};        // axis degenerate 상황에서 쓸 fallback 방향이다.
    };

    struct RibbonInstanceVertex
    {
        Vec4 previousPosition{}; // xyz: segment 시작점 앞 render sample, w: hasPreviousSample flag다.
        Vec4 currentPosition{};  // segment 시작 sample world position이다.
        Vec4 nextPosition{};     // segment 끝 sample world position이다.
        Vec4 nextNextPosition{}; // xyz: segment 끝점 다음 render sample, w: hasNextNextSample flag다.
        Vec2 lifeTime{};         // x: lifetime, y: age.
        Vec4 startColor{};       // segment 시작 색이다.
        Vec4 endColor{};         // segment 끝 색이다.
        Vec4 subUVRect{};        // xy/zw: segment UV 범위다.
        Vec4 segmentParams{};    // x: lifeProgress, y: currentWidth, z: nextWidth, w: alphaScale.
    };

    struct DrawIndexedInstancedIndirectArgs
    {
        uint32 indexCountPerInstance{}; // point index count다.
        uint32 instanceCount{};         // compute가 작성한 segment draw count다.
        uint32 startIndexLocation{};    // index 시작 위치다.
        int32 baseVertexLocation{};     // vertex base offset이다.
        uint32 startInstanceLocation{}; // instance 시작 위치다.
    };

private:
    static constexpr auto kRibbonShaderId{ L"Shader_EffectRibbon" };                                      // ribbon 기본 draw shader id다.
    static constexpr auto kRibbonDistortionShaderId{ L"Shader_EffectRibbonDistortion" };                  // distortion family가 사용할 ribbon shader id다.
    static constexpr auto kRibbonComputeShaderId{ L"ComputeShader_RibbonStrip" };                         // ribbon sample chain을 draw payload로 낮추는 compute shader id다.
    static constexpr auto kFallbackTexturePath{ "Effects/Textures/Shared/DefaultTexture.dds" };          // texture 미지정 fallback이다.
    static constexpr auto kNeutralFlowTexturePath{ "Effects/Textures/Shared/DefaultFlowMap.dds" };       // distortion flow texture 미지정 fallback이다.
    static constexpr uint32 kThreadCountX{ 64 };                                                          // compute shader numthreads.x와 맞춘 dispatch 단위다.
    static constexpr uint32 kMaxDistanceTessellationSubdivision{ 8 };                                     // raw sample interval당 최대 보간 subdivision 수다.

private:
    ComputeRibbonEmitterDesc _desc{};                       // clone 생성 시 전달받은 ribbon emitter runtime 규칙이다.
    PlaybackState _playbackState{ PlaybackState::Delayed }; // delay/playing/completed 중 현재 emitter 재생 상태다.
    float _loopElapsedTime{};                               // 현재 loop 안에서 delay를 포함해 흐른 시간이다.
    uint32 _loopIndex{};                                    // 0부터 시작하는 현재 loop index다.
    vector<RibbonSample> _samples{};                        // ribbon raw self-particle chain 정본이다.
    vector<bool> _burstExecuted{};                          // 현재 loop에서 처리한 Burst 목록이다.
    float _spawnAccumulator{};                              // spawn rate fractional 누적값이다.
    float _materialElapsedTime{};                           // material UV scroll/scalar modulation 시간이다.
    float _visibleRibbonLength{ 1.f };                      // 현재 표시 render chain 길이다.
    uint32 _drawSegmentCount{};                             // 이번 frame draw segment 수다.
    uint32 _nextSampleIndex{};                              // deterministic spawn seed에 사용할 sample index다.

    Shared<ShaderCom> _shader{};                            // ribbon draw shader다.
    Shared<ComputeShaderCom> _computeShader{};              // ribbon strip payload compute shader다.
    Shared<Texture> _mainTexture{};                         // ribbon main texture다.
    Shared<Texture> _noiseTexture{};                        // optional noise texture다.
    Shared<Texture> _maskTexture{};                         // optional mask texture다.
    Shared<Texture> _flowTexture{};                         // optional distortion flow texture다.
    ComPtr<Buffer> _pointVB{};                              // instanced point draw용 단일 vertex buffer다.
    ComPtr<Buffer> _indexBuffer{};                          // instanced point draw용 단일 index buffer다.
    ComPtr<Buffer> _instanceBuffer{};                       // compute 결과를 복사해 draw가 읽는 instance buffer다.
    ComPtr<Buffer> _computeConstantBuffer{};                // compute shader frame parameter buffer다.
    ComPtr<Buffer> _indirectArgsBuffer{};                   // DrawIndexedInstancedIndirect args buffer다.
    Shared<ComputeStructuredBuffer> _sampleInput{};         // CPU render sample chain을 compute가 읽는 SRV buffer다.
    Shared<ComputeStructuredBuffer> _computeOutput{};       // compute가 작성하는 RibbonInstanceVertex buffer다.
    Shared<ComputeStructuredBuffer> _computeArgsOutput{};   // compute가 작성하는 indirect args source buffer다.

private:
    void Reset_PlaybackRuntime();                              // delay/duration/loop runtime 상태를 초기화한다.
    void Advance_Playback(float timeDelta);                    // v0 no-op owner의 재생 clock만 진행한다.

    HRESULT Ready_Components();                                // shader/texture/buffer 리소스를 준비한다.
    HRESULT Ready_Texture();                                   // main texture를 GUID/path/fallback 정책으로 준비한다.
    HRESULT Ready_NoiseTexture();                              // optional noise texture를 준비한다.
    HRESULT Ready_MaskTexture();                               // optional mask texture를 준비한다.
    HRESULT Ready_FlowTexture();                               // distortion flow texture를 준비한다.
    HRESULT Ready_DrawBuffers();                               // draw용 point/index/instance buffer를 만든다.
    HRESULT Ready_ComputeBuffers();                            // compute 입출력/args buffer를 만든다.
    void Update_Samples(float timeDelta);                      // self-particle chain spawn/update/prune을 수행한다.
    void Spawn_RibbonSample(uint32 placementIndex, uint32 placementCount); // seed 기반으로 ribbon sample 하나를 chain head에 추가한다.
    void Build_RenderSamples(vector<RibbonRenderSample>& outSamples) const; // raw chain을 tessellated render sample로 변환한다.
    HRESULT Dispatch_Compute();                                // render sample chain을 ribbon instance payload로 낮춘다.
    HRESULT Update_ComputeInput();                             // CPU render sample chain을 compute 입력으로 업로드한다.
    HRESULT Update_ComputeConstants();                         // ribbon compute 상수 버퍼를 갱신한다.
    HRESULT Reset_IndirectArgs();                              // draw args를 0 segment 상태로 초기화한다.
    HRESULT Copy_ComputeOutput();                              // compute output/args를 draw buffer로 복사한다.
    HRESULT Bind_ShaderResources();                            // camera/material texture 상수를 바인딩한다.
    HRESULT Bind_MaterialResources();                          // 기본 ribbon material resource/uniform을 바인딩한다.
    HRESULT Bind_DistortionResources();                        // distortion ribbon resource/uniform을 바인딩한다.
    bool Can_SubmitRender() const;                             // visible/playback/draw count 기준 render 제출 가능 여부다.
    uint32 Compute_MaxRenderSegmentCount() const;              // maxSampleCount 기준 최대 render segment capacity를 반환한다.

    void Start_NextLoop();                                     // 다음 loop의 delay/elapsed 상태를 준비한다.
    float Resolve_CurrentLoopDelay() const;                    // 현재 loop에 적용할 delay를 반환한다.
    float Resolve_Duration() const;                            // 0 이하 duration으로 인한 무한 루프를 막는 안전 duration을 반환한다.
    bool Has_NextLoop() const;                                 // loopCount 정책상 다음 loop가 있는지 확인한다.
    bool Is_DistortionFamily() const;                          // SpriteDistortion material family 여부다.
    RenderGroup Resolve_RenderGroup() const;                   // material family / override 기준 render group을 반환한다.
    const wchar_t* Resolve_ShaderId() const;                   // material family에 맞는 draw shader id를 반환한다.
    uint32 Resolve_ShaderPassIndex() const;                    // material family와 blend mode에 맞는 shader pass index를 반환한다.
    int Resolve_OpacitySourceIndex() const;                    // opacity source 문자열을 shader index로 변환한다.

    float Sample_SpawnDistribution(const Vec2& range, const PointParticleRandomSeedRuntimeDesc& seed, uint32 salt) const;
    Vec3 Sample_InitialOffset(uint32 seed, uint32 placementIndex, uint32 placementCount, PlaneRadialSample& outPlaneRadialSample) const;
    Vec3 Sample_InitialVelocity(const Vec3& spawnPosition, const PlaneRadialSample& planeRadialSample, uint32 seed) const;
    Vec3 Sample_Acceleration(uint32 seed) const;
    float Sample_Drag(uint32 seed) const;
    Vec4 Sample_Color(
        const Vec4& minColor,
        const Vec4& maxColor,
        const PointParticleRandomSeedRuntimeDesc& colorSeedDesc,
        const PointParticleRandomSeedRuntimeDesc& alphaSeedDesc,
        uint32 seed) const;
    float Evaluate_VelocityScaleByLife(float lifeProgress) const;
    Vec3 Evaluate_AccelerationByLife(const RibbonSample& sample, float lifeProgress) const;
    Vec4 Evaluate_ColorOverLife(const RibbonSample& sample, float lifeProgress) const;
    float Evaluate_WidthScaleByLife(float lifeProgress) const;
    float Evaluate_CompactCurve(
        float lifeProgress,
        const Vec4& times,
        const Vec4& timesBlock1,
        const Vec4& values,
        const Vec4& valuesBlock1,
        uint32 keyCount,
        float fallbackValue) const;
    Vec3 Resolve_LocalVector(const Vec3& localVector) const;
    float Random01(uint32 seed) const;
    float RandomRange(float minValue, float maxValue, uint32 seed) const;

public:
    static Shared<ComputeRibbonEmitter> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
