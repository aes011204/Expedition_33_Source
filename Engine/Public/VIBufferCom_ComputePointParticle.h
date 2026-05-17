#pragma once
#include "Particle_Types.h"
#include "VIBufferCom_Instance.h"

NS_BEGIN(Engine)

class ComputeShaderCom;
class ComputeStructuredBuffer;

class ENGINE_DLL VIBufferCom_ComputePointParticle final : public VIBufferCom_Instance
{
    GENERATED_COMPONENT(VIBufferCom_ComputePointParticle);

public:
    VIBufferCom_ComputePointParticle(const ComPtr<Device>& device, const ComPtr<Context>& context);
    VIBufferCom_ComputePointParticle(const VIBufferCom_ComputePointParticle& prototype);
    ~VIBufferCom_ComputePointParticle() override;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    // desc의 drawMode에 따라 direct 또는 compute-authored indirect draw를 제출한다.
    HRESULT Render() override;

public: //## Behavior::ComputeUpdate
    void Dispatch_Compute(float timeDelta, uint32 spawnRequest, bool killActiveParticles); // spawn/kill 요청을 반영한 뒤 instance payload를 갱신한다.
    void Set_Center(const Vec3& center);                                             // emitter/world transform 변화가 compute spawn center에 반영되도록 갱신한다.

private: //## Static::ComputeSmoke
    static constexpr uint32 kThreadCountX{ 64 };     // compute shader의 numthreads.x와 맞춘 dispatch 단위.
    static constexpr uint32 kDebugArgsLogLimit{ 4 }; // clone별 indirect args readback 로그 횟수 제한.

private: //## Types::IndirectDraw
    // compute shader가 쓰고 indirect draw가 읽는 5-DWORD 공통 args payload.
    struct ComputeIndirectArgs
    {
        uint32 value0{};
        uint32 instanceCount{};
        uint32 value2{};
        int32 value3{};
        uint32 startInstanceLocation{};
    };

    // compute shader가 particle별 수명 상태를 프레임 사이에 유지하기 위한 최소 payload다.
    struct ParticleLifecycleState
    {
        float lifeAge{};
        float lifeMax{};
        uint32 respawnSeed{};
        uint32 active{};
    };

private: //## Data::ParticleDesc
    ComputePointParticleDesc _desc{}; // clone 생성 시 전달받는 compute particle 검증 규칙 저장.

private: //## Data::Compute
    Shared<ComputeShaderCom> _computeShader{};            // instance payload를 쓰는 compute shader.
    Shared<ComputeStructuredBuffer> _computeOutput{};     // compute 결과를 보관하는 structured UAV buffer.
    Shared<ComputeStructuredBuffer> _computeArgsOutput{}; // compute가 작성하는 indirect args source buffer.
    Shared<ComputeStructuredBuffer> _lifecycleState{};    // particle별 age/lifetime을 유지하는 compute-only state buffer.
    float _elapsedTime{};                                 // shader에 전달할 누적 시간.
    float _deltaTime{};                                   // 이번 dispatch의 frame delta time.
    uint32 _spawnRequest{};                               // 이번 dispatch에서 inactive slot을 활성화할 요청 수.
    uint32 _killActiveParticles{};                         // 이번 dispatch에서 active particle을 모두 죽일지 나타낸다.

private: //## Data::GPU
    D3D11_BUFFER_DESC _instanceBufferDesc{}; // compute output 복사 대상 instance VB 생성 설정.
    ComPtr<Buffer> _computeConstantBuffer{}; // 시간/개수 등 frame parameter 전달용 constant buffer.
    ComPtr<Buffer> _indirectArgsBuffer{};    // Draw*InstancedIndirect가 읽는 args destination buffer.
#ifdef _DEBUG
    ComPtr<Buffer> _debugArgsReadbackBuffer{};      // Debug 전용 args staging readback buffer.
    uint32 _debugArgsLogCount{};                    // clone별 args 로그 출력 횟수.
    uint32 _debugLastArgsInstanceCount{ UINT_MAX }; // count 변화가 있을 때만 추가 로그를 남기기 위한 이전 값.
#endif

private: //## Helper::Setup
    HRESULT Ready_VertexBuffer();        // prototype 공용 point vertex 1개를 만든다.
    HRESULT Ready_ComputeShader();       // compute-only shader를 clone 전용 실행 컴포넌트로 준비한다.
    HRESULT Ready_ComputeOutput();       // ParticleInstanceVertex 배열을 쓰는 UAV buffer를 만든다.
    HRESULT Ready_LifecycleState();      // particle별 persistent lifetime state buffer를 만든다.
    HRESULT Ready_InstanceBuffer();      // DrawInstanced가 소비할 default instance VB를 만든다.
    HRESULT Ready_IndexedDrawBuffer();   // indexed indirect 검증용 1-index buffer를 만든다.
    HRESULT Ready_IndirectArgsBuffers(); // compute args source와 draw args destination buffer를 준비한다.
    HRESULT Ready_ConstantBuffer();      // compute shader frame parameter용 constant buffer를 만든다.

private: //## Helper::Runtime
    HRESULT Update_ConstantBuffer();     // 누적 시간과 instance count를 compute shader에 전달한다.
    HRESULT Reset_IndirectArgs();        // compute dispatch 전에 active/activation counters를 0으로 초기화한다.
    HRESULT Copy_OutputToInstanceVB();   // compute output buffer를 instance VB로 복사한다.
    HRESULT Copy_OutputToIndirectArgs(); // compute args source를 indirect args destination으로 복사한다.
    ParticleLifecycleState Make_InitialLifecycleState(uint32 index) const; // 시작 시점의 particle별 수명 상태를 만든다.
#ifdef _DEBUG
    void Debug_LogIndirectArgs(); // Debug 전용으로 compute-authored args 값을 제한적으로 확인한다.
#endif

public:
    static Shared<VIBufferCom_ComputePointParticle> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;
};

NS_END
