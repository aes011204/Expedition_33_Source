#pragma once
#include "EffectRuntime_Types.h"
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
    HRESULT Render() override;

public: //## Behavior::ComputeUpdate
    void Dispatch_Compute(
        float timeDelta,
        uint32 spawnRequest,
        uint32 spawnSerialBase,
        float lifetimeSamplePhase,
        float emitterElapsedTime,
        bool killActiveParticles);
    void Update_RuntimeDesc(const ComputePointParticleDesc& desc);
    void Set_Center(const Vec3& center);
    void Set_EmitterBasis(const Vec3& right, const Vec3& up, const Vec3& look);
    HRESULT Reset_ForEffectReplay();
    bool Has_ActiveParticles() const { return _hasActiveParticles; }
    bool Collect_FollowerSourcePoints(vector<EffectFollowerSourcePoint>& outPoints, uint32 maxPointCount) const;

private: //## Types::IndirectDraw
    struct ComputeIndirectArgs
    {
        uint32 value0{};
        uint32 instanceCount{};
        uint32 value2{};
        int32 value3{};
        uint32 startInstanceLocation{};
    };

    struct ParticleLifecycleState
    {
        float lifeAge{};
        float lifeMax{};
        uint32 respawnSeed{};
        uint32 active{};
        uint32 placementIndex{};
        uint32 placementCount{};
        uint32 reserved0{};
        uint32 reserved1{};
    };

private: //## Static::ComputeDispatch
    static constexpr uint32 kThreadCountX{ 64 };

private: //## Data::ParticleDesc
    ComputePointParticleDesc _desc{};

private: //## Data::Compute
    Shared<ComputeShaderCom> _computeShader{};
    Shared<ComputeStructuredBuffer> _computeOutput{};
    Shared<ComputeStructuredBuffer> _computeArgsOutput{};
    Shared<ComputeStructuredBuffer> _lifecycleState{};
    vector<ParticleLifecycleState> _initialLifecycleState{};
    vector<ParticleInstanceVertex> _emptyInstancePayload{};
    float _elapsedTime{};
    float _deltaTime{};
    float _lifetimeSamplePhase{};
    uint32 _spawnRequest{};
    uint32 _spawnSerialBase{};
    uint32 _killActiveParticles{};
    Vec3 _sourceMotionVelocity{};
    Vec3 _previousSourceMotionCenter{};
    bool _hasPreviousSourceMotionCenter{ false };
    bool _sourceMotionVelocityValid{ false };
    bool _hasActiveParticles{ false };

private: //## Data::GPU
    D3D11_BUFFER_DESC _instanceBufferDesc{};
    ComPtr<Buffer> _computeConstantBuffer{};
    ComPtr<Buffer> _indirectArgsBuffer{};

private: //## Helper::Setup
    HRESULT Ready_VertexBuffer();
    HRESULT Ready_ComputeShader();
    HRESULT Ready_ComputeOutput();
    HRESULT Ready_LifecycleState();
    HRESULT Ready_InstanceBuffer();
    HRESULT Ready_IndexedDrawBuffer();
    HRESULT Ready_IndirectArgsBuffers();
    HRESULT Ready_ConstantBuffer();

private: //## Helper::Runtime
    HRESULT Update_ConstantBuffer();
    HRESULT Reset_IndirectArgs();
    HRESULT Copy_OutputToInstanceVB();
    HRESULT Copy_OutputToIndirectArgs();
    HRESULT Ready_ResetBaseline();
    ParticleLifecycleState Make_InitialLifecycleState(uint32 index) const;

public:
    static Shared<VIBufferCom_ComputePointParticle> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;
};

NS_END
