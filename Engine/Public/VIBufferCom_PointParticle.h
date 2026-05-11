#pragma once
#include "Particle_Types.h"
#include "VIBufferCom_Instance.h"

NS_BEGIN(Engine)

class ENGINE_DLL VIBufferCom_PointParticle final : public VIBufferCom_Instance
{
    GENERATED_COMPONENT(VIBufferCom_PointParticle);

public:
    VIBufferCom_PointParticle(const ComPtr<Device>& device, const ComPtr<Context>& context);
    VIBufferCom_PointParticle(const VIBufferCom_PointParticle& prototype);
    ~VIBufferCom_PointParticle() override;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    HRESULT Render() override;

public: //## Behavior::ParticleUpdate
    // TODO : 이 함수들은 임시로 여기에 두되, 최종적으로는 각 파티클 이펙트 형태마다 동작을 관리하는 것이 더 좋아보임
    void Drop(float timeDelta);   // clone runtime payload를 아래 방향으로 갱신한다.
    void Spread(float timeDelta); // clone runtime payload를 pivot 기준 바깥 방향으로 갱신한다.

private: //## Types::IndirectDraw
    // DrawInstancedIndirect가 읽는 4-DWORD argument layout.
    struct DrawInstancedIndirectArgs
    {
        uint32 vertexCountPerInstance{};
        uint32 instanceCount{};
        uint32 startVertexLocation{};
        uint32 startInstanceLocation{};
    };

    // DrawIndexedInstancedIndirect가 읽는 5-DWORD argument layout.
    struct DrawIndexedInstancedIndirectArgs
    {
        uint32 indexCountPerInstance{};
        uint32 instanceCount{};
        uint32 startIndexLocation{};
        int32 baseVertexLocation{};
        uint32 startInstanceLocation{};
    };

private: //## Data::ParticleDesc
    PointParticleDesc _desc{}; // clone 생성 시 전달받은 point particle 규칙.

private: //## Data::RuntimePayload
    vector<ParticleInstanceVertex> _baselinePayload{}; // loop reset 기준이 되는 초기 payload.
    vector<ParticleInstanceVertex> _runtimePayload{};  // 매 프레임 수정되는 clone 전용 payload.
    vector<float> _speeds{};                           // instance별 이동 속도.

private: //## Data::GPU
    D3D11_BUFFER_DESC _instanceBufferDesc{}; // clone 전용 instance VB 생성 설정.
    ComPtr<Buffer> _indirectArgsBuffer{};    // CPU가 작성한 indirect draw argument buffer.

private: //## Helper::Setup
    HRESULT Ready_VertexBuffer();     // prototype 공용 point vertex 1개를 만든다.
    HRESULT Ready_InstanceSeedData(); // desc 기준 baseline/runtime payload와 speed 배열을 만든다.
    HRESULT Ready_InstanceBuffer();   // clone 전용 GPU instance buffer를 만든다.
    HRESULT Upload_InstancePayload(); // CPU runtime payload를 GPU dynamic buffer로 업로드한다.
    HRESULT Ready_IndexedDrawBuffer(); // indexed indirect 검증용 1-index buffer를 만든다.
    HRESULT Ready_IndirectArgsBuffer(); // CPU-authored indirect args buffer를 만든다.

private: //## Helper::Seed
    static float Pick_RandomFloat(float minValue, float maxValue); // 초기 instance seed 생성용 범위 난수를 세팅한다.

private: //## Helper::Runtime
    static bool Is_Expired(const ParticleInstanceVertex& instance); // lifetime 종료 여부를 판단한다.

public:
    static Shared<VIBufferCom_PointParticle> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;
};

NS_END
