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

private:
    struct TrailHistorySample
    {
        EffectTrailSample sample{}; // provider에서 받은 base/tip world sample이다.
        float age{};                // sample이 누적된 뒤 흐른 시간이다.
    };

    struct TrailInstanceVertex
    {
        Vec4 right{};       // segment 진행 방향과 길이를 담는 instance row다.
        Vec4 up{};          // base-tip 폭 방향과 길이를 담는 instance row다.
        Vec4 look{};        // v0에서는 camera-facing 보정 없이 예비 축으로 둔다.
        Vec4 translation{}; // segment 중심 world position이다.
        Vec2 lifeTime{};    // x: segmentLifetime, y: age.
        Vec4 startColor{};  // segment 시작 색상이다.
        Vec4 endColor{};    // segment 끝 색상이다.
        Vec4 subUVRect{};   // segment별 trail UV 영역이다.
    };

    struct TrailSamplePayload
    {
        Vec4 baseWorldPosition{}; // compute shader가 읽을 trail base world position이다.
        Vec4 tipWorldPosition{};  // compute shader가 읽을 trail tip world position이다.
        Vec4 sampleParams{};      // x: sample age, yzw: reserved.
    };

    struct TrailComputeParams
    {
        uint32 segmentCount{};   // 이번 frame에 유효한 trail segment 수다.
        float segmentLifetime{}; // age fade 기준 lifetime이다.
        float width{};           // strip 폭이다.
        float uvTiling{};        // trail 길이 방향 UV 반복 배율이다.
        Vec4 tint{};             // compute가 start/end color를 만들 때 쓰는 material tint다.
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
    static constexpr auto kTrailShaderId{ "Shader_EffectTrail" };                               // trail segment draw shader id다.
    static constexpr auto kTrailComputeShaderId{ "ComputeShader_TrailStrip" };                  // history를 instance payload로 낮추는 compute shader id다.
    static constexpr auto kFallbackTexturePath{ "Effects/Textures/Shared/DefaultTexture.dds" }; // texture 미지정 시 사용할 기본 텍스처다.
    static constexpr uint32 kThreadCountX{ 64 };                                                // compute shader numthreads.x와 맞춘 dispatch 단위다.

private:
    ComputeTrailEmitterDesc _desc{};       // clone 시점에 받은 trail runtime 설정이다.
    vector<TrailHistorySample> _history{}; // CPU가 보관하는 provider sample history다.
    uint32 _drawSegmentCount{};            // 이번 frame에 그릴 segment 개수다.

    Shared<ShaderCom> _shader{};               // trail draw shader다.
    Shared<ComputeShaderCom> _computeShader{}; // trail payload 생성 compute shader다.
    Shared<Texture> _texture{};                // trail main texture다.

    ComPtr<Buffer> _pointVB{};                            // instanced point draw용 단일 vertex buffer다.
    ComPtr<Buffer> _indexBuffer{};                        // instanced point draw용 단일 index buffer다.
    ComPtr<Buffer> _instanceBuffer{};                     // compute 결과를 복사해 draw가 읽는 instance buffer다.
    ComPtr<Buffer> _computeConstantBuffer{};              // compute shader frame parameter buffer다.
    ComPtr<Buffer> _indirectArgsBuffer{};                 // DrawIndexedInstancedIndirect가 읽는 args destination buffer다.
    Shared<ComputeStructuredBuffer> _historyInput{};      // CPU sample history를 compute가 읽는 SRV buffer다.
    Shared<ComputeStructuredBuffer> _computeOutput{};     // compute가 작성하는 TrailInstanceVertex buffer다.
    Shared<ComputeStructuredBuffer> _computeArgsOutput{}; // compute가 작성하는 indirect args source buffer다.

private:
    HRESULT Ready_Components();
    HRESULT Ready_Texture();
    HRESULT Ready_DrawBuffers();
    HRESULT Ready_ComputeBuffers();
    void Update_History(float timeDelta);
    HRESULT Dispatch_Compute();
    HRESULT Update_ComputeInput();
    HRESULT Update_ComputeConstants();
    HRESULT Reset_IndirectArgs();
    HRESULT Copy_ComputeOutput();
    HRESULT Bind_ShaderResources();

public:
    static Shared<ComputeTrailEmitter> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
