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

public: //## Behavior::ComputeUpdate
    void Dispatch_Compute(float timeDelta); // compute shader로 instance payload를 갱신하고 instance VB로 복사한다.

private: //## Static::ComputeSmoke
    static constexpr uint32 kThreadCountX{ 64 }; // compute shader의 numthreads.x와 맞춘 dispatch 단위.

private: //## Data::ParticleDesc
    ComputePointParticleDesc _desc{}; // clone 생성 시 전달받는 compute particle 검증 규칙 저장.

private: //## Data::Compute
    Shared<ComputeShaderCom> _computeShader{};        // instance payload를 쓰는 compute shader.
    Shared<ComputeStructuredBuffer> _computeOutput{}; // compute 결과를 보관하는 structured UAV buffer.
    float _elapsedTime{};                             // shader에 전달할 누적 시간.

private: //## Data::GPU
    D3D11_BUFFER_DESC _instanceBufferDesc{}; // compute output 복사 대상 instance VB 생성 설정.
    ComPtr<Buffer> _computeConstantBuffer{}; // 시간/개수 등 frame parameter 전달용 constant buffer.

private: //## Helper::Setup
    HRESULT Ready_VertexBuffer();   // prototype 공용 point vertex 1개를 만든다.
    HRESULT Ready_ComputeShader();  // compute-only shader를 clone 전용 실행 컴포넌트로 준비한다.
    HRESULT Ready_ComputeOutput();  // ParticleInstanceVertex 배열을 쓰는 UAV buffer를 만든다.
    HRESULT Ready_InstanceBuffer(); // DrawInstanced가 소비할 default instance VB를 만든다.
    HRESULT Ready_ConstantBuffer(); // compute shader frame parameter용 constant buffer를 만든다.

private: //## Helper::Runtime
    HRESULT Update_ConstantBuffer();   // 누적 시간과 instance count를 compute shader에 전달한다.
    HRESULT Copy_OutputToInstanceVB(); // compute output buffer를 instance VB로 복사한다.

public:
    static Shared<VIBufferCom_ComputePointParticle> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;
};

NS_END
